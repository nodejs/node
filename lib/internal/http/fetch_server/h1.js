'use strict';

// HTTP/1.x backend for the Fetch Server API, built on node:http.

const {
  ArrayPrototypePush,
  PromiseResolve,
  PromiseWithResolvers,
  RegExpPrototypeExec,
  SafeSet,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  StringPrototypeTrim,
} = primordials;

const { URL } = require('internal/url');

const {
  createProtocolError,
  kConnectionReset,
  kGoaway,
} = require('internal/http/fetch_server/errors');

const {
  NodeReadableReader,
} = require('internal/http/fetch_server/node_reader');

const {
  isProhibitedTrailerName,
  listContains,
  listGet,
} = require('internal/http/fetch_server/headers');

const kHopByHop = new SafeSet([
  'connection', 'keep-alive', 'proxy-connection', 'transfer-encoding',
]);
const kAbsoluteForm = /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//;
const kCloseOption = /(?:^|,)\s*close\s*(?:,|$)/i;

function filterRequestTrailers(rawTrailers) {
  const list = [];
  for (let i = 0; i + 1 < rawTrailers.length; i += 2) {
    if (!isProhibitedTrailerName(rawTrailers[i])) {
      ArrayPrototypePush(list, [rawTrailers[i], rawTrailers[i + 1]]);
    }
  }
  return list;
}

/**
 * Builds the request header list from raw headers (Section 8.1).
 * @param {string[]} rawHeaders
 * @returns {Array<[string, string]>}
 */
function buildRequestHeaderList(rawHeaders) {
  const removed = new SafeSet(kHopByHop);
  for (let i = 0; i + 1 < rawHeaders.length; i += 2) {
    if (StringPrototypeToLowerCase(rawHeaders[i]) === 'connection') {
      const options = StringPrototypeSplit(rawHeaders[i + 1], ',');
      for (let j = 0; j < options.length; j++) {
        const option = StringPrototypeToLowerCase(
          StringPrototypeTrim(options[j]));
        if (option !== 'upgrade' && option !== 'te' && option !== '') {
          removed.add(option);
        }
      }
    }
  }
  const list = [];
  for (let i = 0; i + 1 < rawHeaders.length; i += 2) {
    if (!removed.has(StringPrototypeToLowerCase(rawHeaders[i]))) {
      ArrayPrototypePush(list, [rawHeaders[i], rawHeaders[i + 1]]);
    }
  }
  return list;
}

function flattenFields(fields) {
  const flat = [];
  for (let i = 0; i < fields.length; i++) {
    ArrayPrototypePush(flat, fields[i][0], fields[i][1]);
  }
  return flat;
}

function socketAddress(address, port, family) {
  if (address === undefined) return undefined;
  return { address, port, family: family === 'IPv6' ? 'IPv6' : 'IPv4' };
}

class H1Backend {
  req;
  res;
  socket;
  listener;
  kind;
  headerList;
  method;
  url;
  connectProtocol = null;
  // The stream that carries tunnel data for CONNECT and upgrades.
  #tunnelStream;
  #head;
  #reader = null;

  /**
   * @param {object} options
   * @param {import('http').IncomingMessage} options.req
   * @param {import('http').ServerResponse} options.res
   * @param {import('net').Socket} options.socket
   * @param {object} options.listener
   * @param {'fetch'|'connect'|'upgrade'} options.kind
   * @param {import('stream').Duplex} [options.tunnelStream]
   * @param {Buffer} [options.head]
   */
  constructor({ req, res, socket, listener, kind, tunnelStream, head }) {
    this.req = req;
    this.res = res;
    this.socket = socket;
    this.listener = listener;
    this.kind = kind;
    this.#tunnelStream = tunnelStream;
    this.#head = head;
    this.headerList = buildRequestHeaderList(req.rawHeaders);
    this.method = req.method;
    this.url = this.#computeUrl();
    res.strictContentLength = true;
  }

  get httpVersion() {
    return this.req.httpVersion === '1.0' ? '1.0' : '1.1';
  }

  get alpnProtocol() {
    const alpn = this.socket.alpnProtocol;
    return typeof alpn === 'string' && alpn !== '' ? alpn : 'http/1.1';
  }

  get encrypted() {
    return !!this.socket.encrypted;
  }

  get serverName() {
    const name = this.socket.servername;
    return typeof name === 'string' && name !== '' ? name : null;
  }

  remoteAddress() {
    const s = this.socket;
    return socketAddress(s.remoteAddress, s.remotePort, s.remoteFamily);
  }

  localAddress() {
    const s = this.socket;
    return socketAddress(s.localAddress, s.localPort, s.localFamily);
  }

  get hasBody() {
    const req = this.req;
    if (this.kind === 'connect') return false;
    return req.headers['transfer-encoding'] !== undefined ||
      (req.headers['content-length'] !== undefined &&
       req.headers['content-length'] !== '0');
  }

  #computeUrl() {
    const scheme = this.encrypted ? 'https' : 'http';
    const target = this.req.url;
    if (this.kind === 'connect') {
      return new URL(`${scheme}://${target}/`).href;
    }
    if (RegExpPrototypeExec(kAbsoluteForm, target) !== null) {
      return new URL(target).href;
    }
    let host = listGet(this.headerList, 'host');
    if (host === null) {
      // HTTP/1.0 requests can lack a Host header field.
      const local = this.socket.localAddress;
      const port = this.socket.localPort;
      host = this.socket.localFamily === 'IPv6' ?
        `[${local}]:${port}` : `${local}:${port}`;
    }
    return new URL(`${scheme}://${host}${target}`).href;
  }

  /**
   * Associates the backend with its exchange and wires abort detection.
   * @param {import('./exchange').Exchange} exchange
   */
  attach(exchange) {
    const onClose = () => {
      if (exchange.terminal || exchange.tunnel !== null) return;
      if (exchange.state === 'committed' && this.res.writableFinished) {
        // The response was handed to the transport in full.
        exchange.complete();
        return;
      }
      exchange.abort(createProtocolError(
        this.listener.destroying ? kGoaway : kConnectionReset,
        'The connection was lost'));
    };
    this.res.on('error', () => {});
    this.res.on('close', onClose);
    this.socket.on('close', onClose);
    this.socket.on('error', () => {});
    if (this.kind === 'connect' || !this.hasBody) {
      if (this.kind !== 'connect') exchange.completeTrailers([]);
    }
  }

  readBody(exchange) {
    this.#reader ??= new NodeReadableReader(this.req, () => {
      exchange.completeTrailers(filterRequestTrailers(this.req.rawTrailers));
    });
    return this.#reader.read();
  }

  refuseBody() {
    // Node.js reads and discards the remainder of the body after the
    // response has been sent.
  }

  sendContinue(fields) {
    this.res.writeInformation(100, flattenFields(fields));
    this.res._sent100 = true;
  }

  sendInformational(status, fields) {
    this.res.writeInformation(status, flattenFields(fields));
  }

  deny() {
    // HTTP/1.1 has no stream reset: close the connection (Section 7.5).
    this.socket.destroy();
  }

  reset() {
    this.res.destroy();
    this.socket.destroy();
  }

  canSendTrailers(headerList) {
    return this.httpVersion === '1.1' &&
      !listContains(headerList, 'content-length');
  }

  respond(status, statusText, headerList, { hasBody, wantTrailers }) {
    const res = this.res;
    const flat = [];
    let close = false;
    let upgrade = false;
    let hasAltSvc = false;
    for (let i = 0; i < headerList.length; i++) {
      const { 0: name, 1: value } = headerList[i];
      const lower = StringPrototypeToLowerCase(name);
      if (lower === 'connection') {
        if (RegExpPrototypeExec(kCloseOption, value) !== null) close = true;
        continue;
      }
      if (kHopByHop.has(lower)) continue;
      if (lower === 'upgrade') upgrade = true;
      if (lower === 'trailer' && !wantTrailers) continue;
      if (lower === 'alt-svc') hasAltSvc = true;
      if (lower === 'content-length' && status === 204) continue;
      ArrayPrototypePush(flat, name, value);
    }
    if (upgrade) ArrayPrototypePush(flat, 'Connection', 'upgrade');
    const altSvc = this.listener.altSvc;
    if (typeof altSvc === 'string' && this.encrypted && !hasAltSvc) {
      ArrayPrototypePush(flat, 'Alt-Svc', altSvc);
    }
    if (close || this.listener.draining || this.kind !== 'fetch') {
      res.shouldKeepAlive = false;
    }
    res.writeHead(status, statusText === '' ? undefined : statusText, flat);
  }

  write(batch) {
    const res = this.res;
    let ok = true;
    for (let i = 0; i < batch.length; i++) {
      ok = res.write(batch[i]);
    }
    if (ok) return PromiseResolve();
    const { promise, resolve } = PromiseWithResolvers();
    res.once('drain', resolve);
    return promise;
  }

  end(trailers) {
    const res = this.res;
    if (trailers !== null && trailers.length > 0) {
      res.addTrailers(trailers);
    }
    const { promise, resolve } = PromiseWithResolvers();
    res.end(resolve);
    return promise;
  }

  /**
   * Performs the success response for a CONNECT or the upgrade handshake,
   * and returns the transport stream of the tunnel.
   * @returns {Promise<{ stream: import('stream').Duplex }>}
   */
  async acceptTunnel(exchange, fields, protocol, status = 0) {
    const socket = this.socket;
    let head;
    if (this.kind === 'upgrade') {
      if (exchange.pending100) {
        exchange.pending100 = false;
        this.sendContinue([]);
      }
      head = 'HTTP/1.1 101 Switching Protocols\r\n' +
        `Upgrade: ${protocol}\r\nConnection: Upgrade\r\n`;
    } else {
      head = `HTTP/1.1 ${status || 200} Connection Established\r\n`;
    }
    for (let i = 0; i < fields.length; i++) {
      const lower = StringPrototypeToLowerCase(fields[i][0]);
      if (kHopByHop.has(lower) || lower === 'upgrade' ||
          lower === 'content-length') {
        continue;
      }
      head += `${fields[i][0]}: ${fields[i][1]}\r\n`;
    }
    head += '\r\n';
    // The legacy response object is no longer needed.
    this.res.detachSocket?.(socket);
    socket.write(head);
    const stream = this.#tunnelStream ?? socket;
    if (this.#head !== undefined && this.#head.length > 0) {
      stream.unshift(this.#head);
    }
    return { __proto__: null, stream, http2: false };
  }

  goaway() {}
}

module.exports = {
  H1Backend,
  buildRequestHeaderList,
  flattenFields,
};
