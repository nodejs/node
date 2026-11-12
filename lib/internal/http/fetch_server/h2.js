'use strict';

// HTTP/2 backend for the Fetch Server API, built on node:http2.

const {
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  PromiseResolve,
  PromiseWithResolvers,
  SafeSet,
  SafeWeakMap,
  StringPrototypeCharCodeAt,
  StringPrototypeToLowerCase,
} = primordials;

const { URL } = require('internal/url');

const {
  createProtocolError,
  kConnectionReset,
  kGoaway,
  kH2Codes,
  kRequestRejected,
  protocolErrorFromH2,
} = require('internal/http/fetch_server/errors');

const {
  NodeReadableReader,
} = require('internal/http/fetch_server/node_reader');

const {
  isProhibitedTrailerName,
  listContains,
} = require('internal/http/fetch_server/headers');

const kConnectionSpecific = new SafeSet([
  'connection', 'keep-alive', 'proxy-connection', 'transfer-encoding',
  'upgrade',
]);

/**
 * Builds the request header list from HTTP/2 raw headers (Section 8.1).
 * @param {string[]} rawHeaders A flat [name, value, ...] array.
 * @param {string|undefined} authority
 * @returns {Array<[string, string]>}
 */
function buildRequestHeaderList(rawHeaders, authority) {
  const list = [];
  let cookies = null;
  let hasHost = false;
  for (let i = 0; i + 1 < rawHeaders.length; i += 2) {
    const name = rawHeaders[i];
    if (StringPrototypeCharCodeAt(name, 0) === 58 /* ':' */) continue;
    const lower = StringPrototypeToLowerCase(name);
    if (kConnectionSpecific.has(lower)) continue;
    if (lower === 'cookie') {
      cookies ??= [];
      ArrayPrototypePush(cookies, rawHeaders[i + 1]);
      continue;
    }
    if (lower === 'host') hasHost = true;
    ArrayPrototypePush(list, [name, rawHeaders[i + 1]]);
  }
  if (!hasHost && authority !== undefined) {
    ArrayPrototypePush(list, ['host', authority]);
  }
  if (cookies !== null) {
    ArrayPrototypePush(list, ['cookie', ArrayPrototypeJoin(cookies, '; ')]);
  }
  return list;
}

function toHeaderObject(fields, base) {
  const headers = base ?? { __proto__: null };
  for (let i = 0; i < fields.length; i++) {
    const name = StringPrototypeToLowerCase(fields[i][0]);
    if (kConnectionSpecific.has(name)) continue;
    const value = fields[i][1];
    const prev = headers[name];
    if (prev === undefined) {
      headers[name] = value;
    } else if (ArrayIsArray(prev)) {
      ArrayPrototypePush(prev, value);
    } else {
      headers[name] = [prev, value];
    }
  }
  return headers;
}

function socketAddress(address, port, family) {
  if (address === undefined) return undefined;
  return { address, port, family: family === 'IPv6' ? 'IPv6' : 'IPv4' };
}

// Tracks the streams of each session being processed, to select the last
// stream identifier of a GOAWAY frame sent by deny() (Section 7.5).
const sessionStreams = new SafeWeakMap();

class H2Backend {
  stream;
  session;
  listener;
  kind;
  headerList;
  method;
  url;
  connectProtocol;
  #reader = null;

  constructor({ stream, headers, flags, rawHeaders, listener }) {
    this.stream = stream;
    this.session = stream.session;
    this.listener = listener;
    this.method = headers[':method'];
    const authority = headers[':authority'];
    this.connectProtocol = headers[':protocol'];
    this.kind = this.method === 'CONNECT' ? 'connect' : 'fetch';
    this.headerList = buildRequestHeaderList(rawHeaders, authority);
    const scheme = this.encrypted ? 'https' : 'http';
    if (this.kind === 'connect' && this.connectProtocol === undefined) {
      this.url = new URL(`${scheme}://${authority}/`).href;
    } else {
      const host = authority ?? headers.host;
      this.url = new URL(
        `${headers[':scheme'] ?? scheme}://${host}${headers[':path']}`).href;
    }
    let streams = sessionStreams.get(this.session);
    if (streams === undefined) {
      streams = new SafeSet();
      sessionStreams.set(this.session, streams);
    }
    streams.add(stream.id);
    stream.once('close', () => streams.delete(stream.id));
  }

  get httpVersion() {
    return '2';
  }

  get alpnProtocol() {
    return this.encrypted ? 'h2' : 'h2c';
  }

  get encrypted() {
    return !!this.session.encrypted;
  }

  get serverName() {
    const name = this.session.socket?.servername;
    return typeof name === 'string' && name !== '' ? name : null;
  }

  remoteAddress() {
    const s = this.session.socket;
    return socketAddress(s?.remoteAddress, s?.remotePort, s?.remoteFamily);
  }

  localAddress() {
    const s = this.session.socket;
    return socketAddress(s?.localAddress, s?.localPort, s?.localFamily);
  }

  get hasBody() {
    return !this.stream.endAfterHeaders && this.kind !== 'connect';
  }

  attach(exchange) {
    const stream = this.stream;
    const trailers = [];
    stream.on('error', () => {});
    stream.on('trailers', (headers, flags, rawHeaders) => {
      for (let i = 0; i + 1 < rawHeaders.length; i += 2) {
        if (StringPrototypeCharCodeAt(rawHeaders[i], 0) === 58) continue;
        if (!isProhibitedTrailerName(rawHeaders[i])) {
          ArrayPrototypePush(trailers, [rawHeaders[i], rawHeaders[i + 1]]);
        }
      }
    });
    this.trailersList = trailers;
    stream.on('close', () => {
      if (exchange.terminal || exchange.tunnel !== null) return;
      if (exchange.state === 'committed' && stream.writableFinished &&
          !stream.rstCode) {
        exchange.complete();
        return;
      }
      exchange.abort(this.closeReason());
    });
    if (!this.hasBody && this.kind !== 'connect') {
      exchange.completeTrailers([]);
    }
  }

  closeReason() {
    const stream = this.stream;
    const rstCode = stream.rstCode;
    if (rstCode !== undefined && rstCode !== 0 &&
        !this.session.destroyed) {
      return protocolErrorFromH2(rstCode, this.alpnProtocol);
    }
    if (this.session.closed && !this.session.destroyed) {
      return createProtocolError(kGoaway, 'The connection is going away');
    }
    return createProtocolError(
      this.listener.destroying ? kGoaway : kConnectionReset,
      'The connection was lost');
  }

  readBody(exchange) {
    this.#reader ??= new NodeReadableReader(this.stream, () => {
      exchange.completeTrailers(this.trailersList);
    });
    return this.#reader.read();
  }

  refuseBody() {
    const stream = this.stream;
    if (stream.destroyed || stream.readableEnded) return;
    if (stream.headersSent && !stream.writableFinished) {
      // Refuse after the response is complete (RFC 9113 Section 8.1).
      stream.once('finish', () => this.refuseBody());
      return;
    }
    if (stream.headersSent) stream.close(kH2Codes.ERR_HTTP_REQUEST_BODY_REJECTED);
  }

  sendContinue(fields) {
    this.stream.additionalHeaders(toHeaderObject(fields,
                                                 { '__proto__': null, ':status': 100 }));
  }

  sendInformational(status, fields) {
    this.stream.additionalHeaders(toHeaderObject(fields,
                                                 { '__proto__': null, ':status': status }));
  }

  deny(code) {
    const stream = this.stream;
    if (code === kGoaway) {
      const session = this.session;
      let last = 0;
      for (const id of sessionStreams.get(session) ?? []) {
        if (id !== stream.id && id > last) last = id;
      }
      stream.close(kH2Codes[kRequestRejected]);
      try {
        session.goaway(0, last);
      } catch {
        // The session might already be closing.
      }
      return;
    }
    stream.close(kH2Codes[code]);
  }

  reset(code) {
    const stream = this.stream;
    if (!stream.destroyed) stream.close(kH2Codes[code] ?? kH2Codes.ERR_HTTP_INTERNAL_ERROR);
  }

  canSendTrailers() {
    return true;
  }

  respond(status, statusText, headerList, { hasBody, wantTrailers }) {
    const headers = toHeaderObject(headerList,
                                   { '__proto__': null, ':status': status });
    const altSvc = this.listener.altSvc;
    if (typeof altSvc === 'string' && this.encrypted &&
        !listContains(headerList, 'alt-svc')) {
      headers['alt-svc'] = altSvc;
    }
    if (status === 204 || status === 304) {
      delete headers['content-length'];
    }
    this.stream.respond(headers, {
      endStream: !hasBody && !wantTrailers,
      waitForTrailers: wantTrailers,
    });
  }

  write(batch) {
    const stream = this.stream;
    let ok = true;
    for (let i = 0; i < batch.length; i++) {
      ok = stream.write(batch[i]);
    }
    if (ok) return PromiseResolve();
    const { promise, resolve } = PromiseWithResolvers();
    stream.once('drain', resolve);
    return promise;
  }

  end(trailers) {
    const stream = this.stream;
    const { promise, resolve } = PromiseWithResolvers();
    if (trailers !== null) {
      stream.once('wantTrailers', () => {
        stream.sendTrailers(toHeaderObject(trailers));
      });
    }
    if (stream.writableEnded) {
      if (stream.writableFinished) resolve();
      else stream.once('finish', resolve);
    } else {
      stream.end(resolve);
    }
    return promise;
  }

  async acceptTunnel(exchange, fields) {
    const stream = this.stream;
    const headers = toHeaderObject(fields, { '__proto__': null, ':status': 200 });
    delete headers['content-length'];
    stream.respond(headers, { endStream: false });
    return { __proto__: null, stream, http2: true };
  }
}

module.exports = {
  H2Backend,
  toHeaderObject,
};
