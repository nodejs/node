'use strict';

/* eslint-disable no-restricted-syntax */

const {
  NumberIsInteger,
  Promise,
  ArrayIsArray,
  MathMin,
  Error
} = primordials;
const { URL } = require('url');
const net = require('net');
const tls = require('tls');
const EventEmitter = require('events');
const Request = require('./request');
const assert = require('assert');
const { clearTimeout, setTimeout } = require('timers');
const { Buffer } = require('buffer');
const {
  kUrl,
  kWriting,
  kQueue,
  kServerName,
  kSocketTimeout,
  kRequestTimeout,
  kTLSOpts,
  kClosed,
  kDestroyed,
  kPendingIdx,
  kRunningIdx,
  kError,
  kOnDestroyed,
  kPipelining,
  kRetryDelay,
  kRetryTimeout,
  kMaxAbortedPayload,
  kParser,
  kSocket,
  kEnqueue,
  kClient,
  kMaxHeadersSize
} = require('./symbols');
const { HTTPParser } = process.binding('http_parser');

const CRLF = Buffer.from('\r\n', 'ascii');
const TE_CHUNKED = Buffer.from('transfer-encoding: chunked\r\n', 'ascii');
const TE_CHUNKED_EOF = Buffer.from('\r\n0\r\n\r\n', 'ascii');

function nop() {}

const NODE_MAJOR_VERSION = parseInt(process.version.split('.')[0].slice(1));

class ClientBase extends EventEmitter {
  constructor(url, {
    maxAbortedPayload,
    maxHeaderSize,
    socketTimeout,
    requestTimeout,
    pipelining,
    tls
  } = {}) {
    super();

    if (typeof url === 'string') {
      url = new URL(url);
    }

    if (!url || typeof url !== 'object') {
      throw new Error('invalid url');
    }

    if (url.port != null && url.port !== '' &&
        !NumberIsInteger(parseInt(url.port))) {
      throw new Error('invalid port');
    }

    if (url.hostname != null && typeof url.hostname !== 'string') {
      throw new Error('invalid hostname');
    }

    if (!/https?/.test(url.protocol)) {
      throw new Error('invalid protocol');
    }

    if (/\/.+/.test(url.pathname) || url.search || url.hash) {
      throw new Error('invalid url');
    }

    if (maxAbortedPayload != null && !NumberIsInteger(maxAbortedPayload)) {
      throw new Error('invalid maxAbortedPayload');
    }

    if (maxHeaderSize != null && !NumberIsInteger(maxHeaderSize)) {
      throw new Error('invalid maxHeaderSize');
    }

    if (socketTimeout != null && !NumberIsInteger(socketTimeout)) {
      throw new Error('invalid socketTimeout');
    }

    if (requestTimeout != null && !NumberIsInteger(requestTimeout)) {
      throw new Error('invalid requestTimeout');
    }

    this[kSocket] = null;
    this[kPipelining] = pipelining || 1;
    this[kMaxHeadersSize] = maxHeaderSize || 16384;
    this[kUrl] = url;
    this[kSocketTimeout] = socketTimeout == null ? 30e3 : socketTimeout;
    this[kRequestTimeout] = requestTimeout == null ? 30e3 : requestTimeout;
    this[kClosed] = false;
    this[kDestroyed] = false;
    this[kServerName] = null;
    this[kTLSOpts] = tls;
    this[kRetryDelay] = 0;
    this[kRetryTimeout] = null;
    this[kOnDestroyed] = [];
    this[kWriting] = false;
    this[kMaxAbortedPayload] = maxAbortedPayload || 1048576;

    // kQueue is built up of 3 sections separated by
    // the kRunningIdx and kPendingIdx indices.
    // |   complete   |   running   |   pending   |
    //                ^ kRunningIdx ^ kPendingIdx ^ kQueue.length
    // kRunningIdx points to the first running element.
    // kPendingIdx points to the first pending element.
    // This implements a fast queue with an amortized
    // time of O(1).

    this[kQueue] = [];
    this[kRunningIdx] = 0;
    this[kPendingIdx] = 0;
  }

  get pipelining() {
    return this[kPipelining];
  }

  set pipelining(value) {
    this[kPipelining] = value;
    resume(this);
  }

  get connected() {
    return (
      this[kSocket] &&
      this[kSocket].connecting !== true &&
      // Older versions of Node don't set secureConnecting to false.
      (this[kSocket].authorized !== false ||
       this[kSocket].authorizationError
      ) &&
      !this[kSocket].destroyed
    );
  }

  get pending() {
    return this[kQueue].length - this[kPendingIdx];
  }

  get running() {
    return this[kPendingIdx] - this[kRunningIdx];
  }

  get size() {
    return this[kQueue].length - this[kRunningIdx];
  }

  get full() {
    return this.size > this[kPipelining];
  }

  get destroyed() {
    return this[kDestroyed];
  }

  get closed() {
    return this[kClosed];
  }

  [kEnqueue](opts, callback) {
    if (typeof callback !== 'function') {
      throw new Error('invalid callback');
    }

    if (!opts || typeof opts !== 'object') {
      process.nextTick(callback,
                       new Error('invalid opts'), null);
      return;
    }

    if (this[kDestroyed]) {
      process.nextTick(callback, new Error('destroyed'), null);
      return;
    }

    if (this[kClosed]) {
      process.nextTick(callback, new Error('closed'), null);
      return;
    }

    if (opts.requestTimeout == null && this[kRequestTimeout]) {
      // TODO: Avoid copy.
      opts = { ...opts, requestTimeout: this[kRequestTimeout] };
    }

    let request;
    try {
      request = new Request(opts, this[kUrl].hostname, callback);
    } catch (err) {
      process.nextTick(callback, err, null);
      return;
    }

    this[kQueue].push(request);

    resume(this);

    return request;
  }

  close(callback) {
    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.close((err, data) => {
          return err ? reject(err) : resolve(data);
        });
      });
    }

    if (typeof callback !== 'function') {
      throw new Error('invalid callback');
    }

    if (this[kDestroyed]) {
      process.nextTick(callback, new Error('destroyed'), null);
      return;
    }

    this[kClosed] = true;
    this[kOnDestroyed].push(callback);

    resume(this);
  }

  destroy(err, callback) {
    if (typeof err === 'function') {
      callback = err;
      err = null;
    }

    if (callback === undefined) {
      return new Promise((resolve, reject) => {
        this.destroy(err, (err, data) => {
          return err ? reject(err) : resolve(data);
        });
      });
    }

    if (typeof callback !== 'function') {
      throw new Error('invalid callback');
    }

    if (this[kDestroyed]) {
      if (this[kOnDestroyed]) {
        this[kOnDestroyed].push(callback);
      } else {
        process.nextTick(callback, null, null);
      }
      return;
    }

    clearTimeout(this[kRetryTimeout]);
    this[kRetryTimeout] = null;
    this[kClosed] = true;
    this[kDestroyed] = true;
    this[kOnDestroyed].push(callback);

    const onDestroyed = () => {
      const callbacks = this[kOnDestroyed];
      this[kOnDestroyed] = null;
      for (const callback of callbacks) {
        callback(null, null);
      }
    };

    if (!this[kSocket] || this[kSocket][kClosed]) {
      process.nextTick(onDestroyed);
    } else {
      this[kSocket]
        .on('close', onDestroyed)
        .destroy(err);
    }

    // Resume will invoke callbacks and must happen in nextTick
    // TODO: Implement in a more elegant way.
    process.nextTick(resume, this);
  }
}

class Parser extends HTTPParser {
  constructor(client, socket) {
    if (NODE_MAJOR_VERSION >= 12) {
      super();
      this.initialize(
        HTTPParser.RESPONSE,
        {},
        client[kMaxHeadersSize],
        false,
        0
      );
    } else {
      super(HTTPParser.RESPONSE, false);
    }

    this.client = client;
    this.socket = socket;
    this.resumeSocket = () => socket.resume();
    this.read = 0;
    this.body = null;
  }

  [HTTPParser.kOnHeaders]() {
    // TODO: Handle trailers.
  }

  [HTTPParser.kOnHeadersComplete](versionMajor, versionMinor, headers, method,
                                  url, statusCode, statusMessage, upgrade,
                                  shouldKeepAlive) {
    const { client, resumeSocket } = this;
    const request = client[kQueue][client[kRunningIdx]];
    const { signal, opaque } = request;
    const skipBody = request.method === 'HEAD';

    assert(!this.read);
    assert(!this.body);

    if (statusCode === 101) {
      request.invoke(new Error('101 response not supported'));
      return true;
    }

    if (statusCode < 200) {
      // TODO: Informational response.
      return true;
    }

    let body = request.invoke(null, {
      statusCode,
      headers: parseHeaders(headers),
      opaque,
      resume: resumeSocket
    });

    if (body && skipBody) {
      body(null, null);
      body = null;
    }

    if (body) {
      this.body = body;

      if (signal) {
        signal.once('error', body);
      }
    } else {
      this.next();
    }

    return skipBody;
  }

  [HTTPParser.kOnBody](chunk, offset, length) {
    this.read += length;

    const { client, socket, body, read } = this;

    const ret = body ? body(null, chunk.slice(offset, offset + length)) : null;

    if (ret == null && read > client[kMaxAbortedPayload]) {
      socket.destroy();
    } else if (ret === false) {
      socket.pause();
    }
  }

  [HTTPParser.kOnMessageComplete]() {
    const { body } = this;

    this.read = 0;
    this.body = null;

    if (body) {
      body(null, null);
      this.next();
    }
  }

  next() {
    const { client, resumeSocket } = this;

    resumeSocket();

    client[kQueue][client[kRunningIdx]++] = null;

    resume(client);
  }

  destroy(err) {
    const { client, body } = this;

    assert(err);

    if (client[kRunningIdx] >= client[kPendingIdx]) {
      assert(!body);
      return;
    }

    this.read = 0;
    this.body = null;

    // Retry all idempotent requests except for the one
    // at the head of the pipeline.

    const retryRequests = [];
    const errorRequests = [];

    errorRequests.push(client[kQueue][client[kRunningIdx]++]);

    const requests = client[kQueue].slice(client[kRunningIdx],
                                          client[kPendingIdx]);
    for (const request of requests) {
      const { idempotent, body } = request;
      if (idempotent && (!body || typeof body.pipe !== 'function')) {
        retryRequests.push(request);
      } else {
        errorRequests.push(request);
      }
    }

    client[kQueue].splice(0, client[kPendingIdx], ...retryRequests);

    client[kPendingIdx] = 0;
    client[kRunningIdx] = 0;

    if (body) {
      body(err, null);
    }

    for (const request of errorRequests) {
      request.invoke(err, null);
    }

    this.close();
  }
}

function connect(client) {
  assert(!client[kSocket]);
  assert(!client[kRetryTimeout]);

  const { protocol, port, hostname } = client[kUrl];
  const servername = client[kServerName] ||
    (client[kTLSOpts] && client[kTLSOpts].servername);
  const socket = protocol === 'https:' ?
    tls.connect(port || 443, hostname, {
      ...client[kTLSOpts],
      servername
    }) : net.connect(port || 80, hostname);

  client[kSocket] = socket;

  socket[kClient] = client;
  socket[kParser] = new Parser(client, socket);
  socket[kClosed] = false;
  socket[kError] = null;
  socket.setTimeout(client[kSocketTimeout], function() {
    this.destroy(new Error('socket timeout'));
  });
  socket
    .setNoDelay(true)
    .on(protocol === 'https:' ? 'secureConnect' : 'connect', function() {
      const client = this[kClient];

      client[kRetryDelay] = 0;
      client.emit('connect');
      resume(client);
    })
    .on('data', function(chunk) {
      const parser = this[kParser];

      const err = parser.execute(chunk);
      if (err instanceof Error && !this.destroyed) {
        this.destroy(err);
      }
    })
    .on('error', function(err) {
      if (err.code === 'ERR_TLS_CERT_ALTNAME_INVALID') {
        assert(!client.running);
        while (client.pending &&
               client[kQueue][client[kPendingIdx]].servername === servername) {
          client[kQueue][client[kPendingIdx]++].invoke(err, null);
        }
      } else if (
        !client.running &&
        err.code !== 'ECONNRESET' &&
        err.code !== 'ECONNREFUSED' &&
        err.code !== 'EHOSTUNREACH' &&
        err.code !== 'EHOSTDOWN' &&
        err.message !== 'closed' &&
        err.message !== 'other side closed'
      ) {
        // Error is not caused by running request and not a recoverable
        // socket error.
        for (const request of client[kQueue].splice(client[kPendingIdx])) {
          request.invoke(err, null);
        }
      }

      this[kError] = err;
    })
    .on('end', function() {
      this.destroy(new Error('other side closed'));
    })
    .on('close', function() {
      const client = this[kClient];
      const parser = this[kParser];

      this[kClosed] = true;

      if (!socket[kError]) {
        socket[kError] = new Error('closed');
      }

      const err = socket[kError];

      parser.destroy(err);

      resume(client);

      if (client.destroyed) {
        return;
      }

      // reset events
      client[kSocket]
        .removeAllListeners('data')
        .removeAllListeners('end')
        .removeAllListeners('finish')
        .removeAllListeners('error');
      client[kSocket]
        .on('error', nop);
      client[kSocket] = null;

      if (client.pending > 0) {
        if (client[kRetryDelay]) {
          client[kRetryTimeout] = setTimeout(() => {
            client[kRetryTimeout] = null;
            connect(client);
          }, client[kRetryDelay]);
          client[kRetryDelay] = MathMin(client[kRetryDelay] * 2,
                                        client[kSocketTimeout]);
        } else {
          connect(client);
          client[kRetryDelay] = 1e3;
        }
      }

      client.emit('disconnect', err);
    });
}

function resume(client) {
  while (true) {
    if (client[kDestroyed]) {
      const requests = client[kQueue].splice(client[kPendingIdx]);
      for (const request of requests) {
        request.invoke(new Error('destroyed'), null);
      }
      return;
    }

    if (client.size === 0) {
      if (client[kClosed]) {
        client.destroy(nop);
      }
      if (client[kRunningIdx] > 0) {
        client[kQueue].length = 0;
        client[kPendingIdx] = 0;
        client[kRunningIdx] = 0;
      }
      return;
    }

    if (client[kRunningIdx] > 256) {
      client[kQueue].splice(0, client[kRunningIdx]);
      client[kPendingIdx] -= client[kRunningIdx];
      client[kRunningIdx] = 0;
    }

    if (client.running >= client.pipelining) {
      return;
    }

    if (!client.pending) {
      return;
    }

    const request = client[kQueue][client[kPendingIdx]];

    if (!request.callback) {
      // Request was aborted.
      // TODO: Avoid splice one by one.
      client[kQueue].splice(client[kPendingIdx], 1);
      continue;
    }

    if (client[kServerName] !== request.servername) {
      if (client.running) {
        return;
      }

      client[kServerName] = request.servername;

      if (client[kSocket]) {
        client[kSocket].destroy();
      }
    }

    if (!client[kSocket] && !client[kRetryTimeout]) {
      connect(client);
      return;
    }

    if (!client.connected) {
      return;
    }

    if (client[kWriting]) {
      return;
    }

    if (!request.idempotent && client.running) {
      // Non-idempotent request cannot be retried.
      // Ensure that no other requests are inflight and
      // could cause failure.
      return;
    }

    if (request.streaming && client.running) {
      // Request with stream body can error while other requests
      // are inflight and indirectly error those as well.
      // Ensure this doesn't happen by waiting for inflight
      // to complete before dispatching.

      // TODO: This is to strict. Would be better if when
      // request body fails, the client waits for inflight
      // before resetting the connection.
      return;
    }

    client[kPendingIdx]++;

    write(client, request);

    // Release memory for no longer required properties.
    request.headers = null;
    request.body = null;
  }
}

function write(client, {
  header,
  body,
  streaming,
  chunked,
  signal
}) {
  const socket = client[kSocket];

  socket.cork();
  socket.write(header);

  if (!body) {
    socket.write(CRLF);
    socket.uncork();
  } else if (!streaming) {
    socket.write(CRLF);
    socket.write(body);
    socket.write(CRLF);
    socket.uncork();
  } else {
    if (chunked) {
      socket.write(TE_CHUNKED);
    } else {
      socket.write(CRLF);
    }

    const onData = (chunk) => {
      if (chunked) {
        socket.write(`\r\n${Buffer.byteLength(chunk).toString(16)}\r\n`,
                     'ascii');
      }
      if (!socket.write(chunk) && body.pause) {
        body.pause();
      }
    };
    const onDrain = () => {
      if (body.resume) {
        body.resume();
      }
    };
    const onAbort = () => {
      onFinished(new Error('aborted'));
    };

    let finished = false;
    const onFinished = (err) => {
      if (finished) {
        return;
      }
      finished = true;

      err = err || socket[kError];

      if (signal) {
        signal.removeListener('error', onFinished);
      }

      socket
        .removeListener('drain', onDrain)
        .removeListener('error', onFinished)
        .removeListener('close', onFinished);
      body
        .removeListener('data', onData)
        .removeListener('end', onFinished)
        .removeListener('error', onFinished)
        .removeListener('close', onAbort)
        .on('error', nop);

      if (err) {
        if (typeof body.destroy === 'function' && !body.destroyed) {
          body.destroy(err);
        }

        if (!socket.destroyed) {
          assert(client.running);
          socket.destroy(err);
        }
      } else if (chunked) {
        socket.write(TE_CHUNKED_EOF);
      } else {
        socket.write(CRLF);
      }

      client[kWriting] = false;
      resume(client);
    };

    if (signal) {
      signal.on('error', onFinished);
    }

    body
      .on('data', onData)
      .on('end', onFinished)
      .on('error', onFinished)
      .on('close', onAbort);

    socket
      .on('drain', onDrain)
      .on('error', onFinished)
      .on('close', onFinished)
      .uncork();

    client[kWriting] = true;
  }
}

function parseHeaders(headers) {
  const obj = {};
  for (var i = 0; i < headers.length; i += 2) {
    var key = headers[i].toLowerCase();
    var val = obj[key];
    if (!val) {
      obj[key] = headers[i + 1];
    } else {
      if (!ArrayIsArray(val)) {
        val = [val];
        obj[key] = val;
      }
      val.push(headers[i + 1]);
    }
  }
  return obj;
}

module.exports = ClientBase;
