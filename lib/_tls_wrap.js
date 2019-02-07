// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

require('internal/util').assertCrypto();

const assert = require('internal/assert');
const crypto = require('crypto');
const net = require('net');
const tls = require('tls');
const util = require('util');
const common = require('_tls_common');
const JSStreamSocket = require('internal/js_stream_socket');
const { Buffer } = require('buffer');
const debug = util.debuglog('tls');
const { TCP, constants: TCPConstants } = internalBinding('tcp_wrap');
const tls_wrap = internalBinding('tls_wrap');
const { Pipe, constants: PipeConstants } = internalBinding('pipe_wrap');
const { owner_symbol } = require('internal/async_hooks').symbols;
const { SecureContext: NativeSecureContext } = internalBinding('crypto');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_MULTIPLE_CALLBACK,
  ERR_SOCKET_CLOSED,
  ERR_TLS_DH_PARAM_SIZE,
  ERR_TLS_HANDSHAKE_TIMEOUT,
  ERR_TLS_RENEGOTIATE,
  ERR_TLS_RENEGOTIATION_DISABLED,
  ERR_TLS_REQUIRED_SERVER_NAME,
  ERR_TLS_SESSION_ATTACK,
  ERR_TLS_SNI_FROM_SERVER
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');
const kConnectOptions = Symbol('connect-options');
const kDisableRenegotiation = Symbol('disable-renegotiation');
const kErrorEmitted = Symbol('error-emitted');
const kHandshakeTimeout = Symbol('handshake-timeout');
const kRes = Symbol('res');
const kSNICallback = Symbol('snicallback');

const noop = () => {};

let ipServernameWarned = false;

// Server side times how long a handshake is taking to protect against slow
// handshakes being used for DoS.
function onhandshakestart(now) {
  debug('onhandshakestart');

  const { lastHandshakeTime } = this;
  assert(now >= lastHandshakeTime,
         `now (${now}) < lastHandshakeTime (${lastHandshakeTime})`);

  this.lastHandshakeTime = now;

  // If this is the first handshake we can skip the rest of the checks.
  if (lastHandshakeTime === 0)
    return;

  if ((now - lastHandshakeTime) >= tls.CLIENT_RENEG_WINDOW * 1000)
    this.handshakes = 1;
  else
    this.handshakes++;

  const owner = this[owner_symbol];
  if (this.handshakes > tls.CLIENT_RENEG_LIMIT) {
    owner._emitTLSError(new ERR_TLS_SESSION_ATTACK());
    return;
  }

  if (owner[kDisableRenegotiation])
    owner._emitTLSError(new ERR_TLS_RENEGOTIATION_DISABLED());
}

function onhandshakedone() {
  debug('onhandshakedone');

  const owner = this[owner_symbol];

  // `newSession` callback wasn't called yet
  if (owner._newSessionPending) {
    owner._securePending = true;
    return;
  }

  owner._finishInit();
}


function loadSession(hello) {
  const owner = this[owner_symbol];

  var once = false;
  function onSession(err, session) {
    if (once)
      return owner.destroy(new ERR_MULTIPLE_CALLBACK());
    once = true;

    if (err)
      return owner.destroy(err);

    if (owner._handle === null)
      return owner.destroy(new ERR_SOCKET_CLOSED());

    owner._handle.loadSession(session);
    // Session is loaded. End the parser to allow handshaking to continue.
    owner._handle.endParser();
  }

  if (hello.sessionId.length <= 0 ||
      hello.tlsTicket ||
      owner.server &&
      !owner.server.emit('resumeSession', hello.sessionId, onSession)) {
    // Sessions without identifiers can't be resumed.
    // Sessions with tickets can be resumed directly from the ticket, no server
    // session storage is necessary.
    // Without a call to a resumeSession listener, a session will never be
    // loaded, so end the parser to allow handshaking to continue.
    owner._handle.endParser();
  }
}


function loadSNI(info) {
  const owner = this[owner_symbol];
  const servername = info.servername;
  if (!servername || !owner._SNICallback)
    return requestOCSP(owner, info);

  let once = false;
  owner._SNICallback(servername, (err, context) => {
    if (once)
      return owner.destroy(new ERR_MULTIPLE_CALLBACK());
    once = true;

    if (err)
      return owner.destroy(err);

    if (owner._handle === null)
      return owner.destroy(new ERR_SOCKET_CLOSED());

    // TODO(indutny): eventually disallow raw `SecureContext`
    if (context)
      owner._handle.sni_context = context.context || context;

    requestOCSP(owner, info);
  });
}


function requestOCSP(socket, info) {
  if (!info.OCSPRequest || !socket.server)
    return requestOCSPDone(socket);

  let ctx = socket._handle.sni_context;

  if (!ctx) {
    ctx = socket.server._sharedCreds;

    // TLS socket is using a `net.Server` instead of a tls.TLSServer.
    // Some TLS properties like `server._sharedCreds` will not be present
    if (!ctx)
      return requestOCSPDone(socket);
  }

  // TODO(indutny): eventually disallow raw `SecureContext`
  if (ctx.context)
    ctx = ctx.context;

  if (socket.server.listenerCount('OCSPRequest') === 0) {
    return requestOCSPDone(socket);
  }

  let once = false;
  const onOCSP = (err, response) => {
    if (once)
      return socket.destroy(new ERR_MULTIPLE_CALLBACK());
    once = true;

    if (err)
      return socket.destroy(err);

    if (socket._handle === null)
      return socket.destroy(new ERR_SOCKET_CLOSED());

    if (response)
      socket._handle.setOCSPResponse(response);
    requestOCSPDone(socket);
  };

  socket.server.emit('OCSPRequest',
                     ctx.getCertificate(),
                     ctx.getIssuer(),
                     onOCSP);
}

function requestOCSPDone(socket) {
  try {
    socket._handle.certCbDone();
  } catch (e) {
    socket.destroy(e);
  }
}


function onnewsessionclient(sessionId, session) {
  debug('client onnewsessionclient', sessionId, session);
  const owner = this[owner_symbol];
  owner.emit('session', session);
}

function onnewsession(sessionId, session) {
  debug('onnewsession');
  const owner = this[owner_symbol];

  // XXX(sam) no server to emit the event on, but handshake won't continue
  // unless newSessionDone() is called, should it be?
  if (!owner.server)
    return;

  var once = false;
  const done = () => {
    debug('onnewsession done');
    if (once)
      return;
    once = true;

    if (owner._handle === null)
      return owner.destroy(new ERR_SOCKET_CLOSED());

    this.newSessionDone();

    owner._newSessionPending = false;
    if (owner._securePending)
      owner._finishInit();
    owner._securePending = false;
  };

  owner._newSessionPending = true;
  if (!owner.server.emit('newSession', sessionId, session, done))
    done();
}


function onocspresponse(resp) {
  this[owner_symbol].emit('OCSPResponse', resp);
}

function onerror(err) {
  const owner = this[owner_symbol];

  if (owner._hadError)
    return;

  owner._hadError = true;

  // Destroy socket if error happened before handshake's finish
  if (!owner._secureEstablished) {
    // When handshake fails control is not yet released,
    // so self._tlsError will return null instead of actual error
    owner.destroy(err);
  } else if (owner._tlsOptions.isServer &&
             owner._rejectUnauthorized &&
             /peer did not return a certificate/.test(err.message)) {
    // Ignore server's authorization errors
    owner.destroy();
  } else {
    // Throw error
    owner._emitTLSError(err);
  }
}

// Used by both client and server TLSSockets to start data flowing from _handle,
// read(0) causes a StreamBase::ReadStart, via Socket._read.
function initRead(tlsSocket, socket) {
  // If we were destroyed already don't bother reading
  if (!tlsSocket._handle)
    return;

  // Socket already has some buffered data - emulate receiving it
  if (socket && socket.readableLength) {
    var buf;
    while ((buf = socket.read()) !== null)
      tlsSocket._handle.receive(buf);
  }

  tlsSocket.read(0);
}

/**
 * Provides a wrap of socket stream to do encrypted communication.
 */

function TLSSocket(socket, opts) {
  const tlsOptions = { ...opts };

  if (tlsOptions.ALPNProtocols)
    tls.convertALPNProtocols(tlsOptions.ALPNProtocols, tlsOptions);

  this._tlsOptions = tlsOptions;
  this._secureEstablished = false;
  this._securePending = false;
  this._newSessionPending = false;
  this._controlReleased = false;
  this._SNICallback = null;
  this.servername = null;
  this.alpnProtocol = null;
  this.authorized = false;
  this.authorizationError = null;
  this[kRes] = null;

  var wrap;
  if ((socket instanceof net.Socket && socket._handle) || !socket) {
    // 1. connected socket
    // 2. no socket, one will be created with net.Socket().connect
    wrap = socket;
  } else {
    // 3. socket has no handle so it is js not c++
    // 4. unconnected sockets are wrapped
    // TLS expects to interact from C++ with a net.Socket that has a C++ stream
    // handle, but a JS stream doesn't have one. Wrap it up to make it look like
    // a socket.
    wrap = new JSStreamSocket(socket);
    wrap.once('close', () => this.destroy());
  }

  // Just a documented property to make secure sockets
  // distinguishable from regular ones.
  this.encrypted = true;

  net.Socket.call(this, {
    handle: this._wrapHandle(wrap),
    allowHalfOpen: socket && socket.allowHalfOpen,
    readable: false,
    writable: false
  });

  // Proxy for API compatibility
  this.ssl = this._handle;  // C++ TLSWrap object

  this.on('error', this._tlsError);

  this._init(socket, wrap);

  // Make sure to setup all required properties like: `connecting` before
  // starting the flow of the data
  this.readable = true;
  this.writable = true;

  // Read on next tick so the caller has a chance to setup listeners
  process.nextTick(initRead, this, socket);
}
util.inherits(TLSSocket, net.Socket);
exports.TLSSocket = TLSSocket;

var proxiedMethods = [
  'ref', 'unref', 'open', 'bind', 'listen', 'connect', 'bind6',
  'connect6', 'getsockname', 'getpeername', 'setNoDelay', 'setKeepAlive',
  'setSimultaneousAccepts', 'setBlocking',

  // PipeWrap
  'setPendingInstances'
];

// Proxy HandleWrap, PipeWrap and TCPWrap methods
function makeMethodProxy(name) {
  return function methodProxy(...args) {
    if (this._parent[name])
      return this._parent[name].apply(this._parent, args);
  };
}
for (var n = 0; n < proxiedMethods.length; n++) {
  tls_wrap.TLSWrap.prototype[proxiedMethods[n]] =
    makeMethodProxy(proxiedMethods[n]);
}

tls_wrap.TLSWrap.prototype.close = function close(cb) {
  let ssl;
  if (this[owner_symbol]) {
    ssl = this[owner_symbol].ssl;
    this[owner_symbol].ssl = null;
  }

  // Invoke `destroySSL` on close to clean up possibly pending write requests
  // that may self-reference TLSWrap, leading to leak
  const done = () => {
    if (ssl) {
      ssl.destroySSL();
      if (ssl._secureContext.singleUse) {
        ssl._secureContext.context.close();
        ssl._secureContext.context = null;
      }
    }
    if (cb)
      cb();
  };

  if (this._parentWrap && this._parentWrap._handle === this._parent) {
    this._parentWrap.once('close', done);
    return this._parentWrap.destroy();
  }
  return this._parent.close(done);
};

TLSSocket.prototype.disableRenegotiation = function disableRenegotiation() {
  this[kDisableRenegotiation] = true;
};

TLSSocket.prototype._wrapHandle = function(wrap) {
  var handle;

  if (wrap)
    handle = wrap._handle;

  var options = this._tlsOptions;
  if (!handle) {
    handle = options.pipe ?
      new Pipe(PipeConstants.SOCKET) :
      new TCP(TCPConstants.SOCKET);
    handle[owner_symbol] = this;
  }

  // Wrap socket's handle
  const context = options.secureContext ||
                  options.credentials ||
                  tls.createSecureContext(options);
  const externalStream = handle._externalStream;
  assert(typeof externalStream === 'object',
         'handle must be a LibuvStreamWrap');
  assert(context.context instanceof NativeSecureContext,
         'context.context must be a NativeSecureContext');
  const res = tls_wrap.wrap(externalStream,
                            context.context,
                            !!options.isServer);
  res._parent = handle;  // C++ "wrap" object: TCPWrap, JSStream, ...
  res._parentWrap = wrap;  // JS object: net.Socket, JSStreamSocket, ...
  res._secureContext = context;
  res.reading = handle.reading;
  this[kRes] = res;
  defineHandleReading(this, handle);

  this.on('close', onSocketCloseDestroySSL);

  return res;
};

// This eliminates a cyclic reference to TLSWrap
// Ref: https://github.com/nodejs/node/commit/f7620fb96d339f704932f9bb9a0dceb9952df2d4
function defineHandleReading(socket, handle) {
  Object.defineProperty(handle, 'reading', {
    get: () => {
      return socket[kRes].reading;
    },
    set: (value) => {
      socket[kRes].reading = value;
    }
  });
}

function onSocketCloseDestroySSL() {
  // Make sure we are not doing it on OpenSSL's stack
  setImmediate(destroySSL, this);
  this[kRes] = null;
}

function destroySSL(self) {
  self._destroySSL();
}

TLSSocket.prototype._destroySSL = function _destroySSL() {
  if (!this.ssl) return;
  this.ssl.destroySSL();
  if (this.ssl._secureContext.singleUse) {
    this.ssl._secureContext.context.close();
    this.ssl._secureContext.context = null;
  }
  this.ssl = null;
};

TLSSocket.prototype._init = function(socket, wrap) {
  var options = this._tlsOptions;
  var ssl = this._handle;

  this.server = options.server;

  // Clients (!isServer) always request a cert, servers request a client cert
  // only on explicit configuration.
  const requestCert = !!options.requestCert || !options.isServer;
  const rejectUnauthorized = !!options.rejectUnauthorized;

  this._requestCert = requestCert;
  this._rejectUnauthorized = rejectUnauthorized;
  if (requestCert || rejectUnauthorized)
    ssl.setVerifyMode(requestCert, rejectUnauthorized);

  if (options.isServer) {
    ssl.onhandshakestart = onhandshakestart;
    ssl.onhandshakedone = onhandshakedone;
    ssl.onclienthello = loadSession;
    ssl.oncertcb = loadSNI;
    ssl.onnewsession = onnewsession;
    ssl.lastHandshakeTime = 0;
    ssl.handshakes = 0;

    if (this.server) {
      if (this.server.listenerCount('resumeSession') > 0 ||
          this.server.listenerCount('newSession') > 0) {
        // Also starts the client hello parser as a side effect.
        ssl.enableSessionCallbacks();
      }
      if (this.server.listenerCount('OCSPRequest') > 0)
        ssl.enableCertCb();
    }
  } else {
    ssl.onhandshakestart = noop;
    ssl.onhandshakedone = this._finishInit.bind(this);
    ssl.onocspresponse = onocspresponse;

    if (options.session)
      ssl.setSession(options.session);

    ssl.onnewsession = onnewsessionclient;

    // Only call .onnewsession if there is a session listener.
    this.on('newListener', newListener);

    function newListener(event) {
      if (event !== 'session')
        return;

      ssl.enableSessionCallbacks();

      // Remover this listener since its no longer needed.
      this.removeListener('newListener', newListener);
    }
  }

  ssl.onerror = onerror;

  // If custom SNICallback was given, or if
  // there're SNI contexts to perform match against -
  // set `.onsniselect` callback.
  if (options.isServer &&
      options.SNICallback &&
      (options.SNICallback !== SNICallback ||
       (options.server && options.server._contexts.length))) {
    assert(typeof options.SNICallback === 'function');
    this._SNICallback = options.SNICallback;
    ssl.enableCertCb();
  }

  if (options.ALPNProtocols) {
    // keep reference in secureContext not to be GC-ed
    ssl._secureContext.alpnBuffer = options.ALPNProtocols;
    ssl.setALPNProtocols(ssl._secureContext.alpnBuffer);
  }

  if (options.handshakeTimeout > 0)
    this.setTimeout(options.handshakeTimeout, this._handleTimeout);

  if (socket instanceof net.Socket) {
    this._parent = socket;

    // To prevent assertion in afterConnect() and properly kick off readStart
    this.connecting = socket.connecting || !socket._handle;
    socket.once('connect', () => {
      this.connecting = false;
      this.emit('connect');
    });
  }

  // Assume `tls.connect()`
  if (wrap) {
    wrap.on('error', (err) => this._emitTLSError(err));
  } else {
    assert(!socket);
    this.connecting = true;
  }
};

TLSSocket.prototype.renegotiate = function(options, callback) {
  if (options === null || typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  if (callback !== undefined && typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK();

  if (this.destroyed)
    return;

  let requestCert = !!this._requestCert;
  let rejectUnauthorized = !!this._rejectUnauthorized;

  if (options.requestCert !== undefined)
    requestCert = !!options.requestCert;
  if (options.rejectUnauthorized !== undefined)
    rejectUnauthorized = !!options.rejectUnauthorized;

  if (requestCert !== this._requestCert ||
      rejectUnauthorized !== this._rejectUnauthorized) {
    this._handle.setVerifyMode(requestCert, rejectUnauthorized);
    this._requestCert = requestCert;
    this._rejectUnauthorized = rejectUnauthorized;
  }
  // Ensure that we'll cycle through internal openssl's state
  this.write('');

  if (!this._handle.renegotiate()) {
    if (callback) {
      process.nextTick(callback, new ERR_TLS_RENEGOTIATE());
    }
    return false;
  }

  // Ensure that we'll cycle through internal openssl's state
  this.write('');

  if (callback) {
    this.once('secure', () => callback(null));
  }

  return true;
};

TLSSocket.prototype.setMaxSendFragment = function setMaxSendFragment(size) {
  return this._handle.setMaxSendFragment(size) === 1;
};

TLSSocket.prototype._handleTimeout = function() {
  this._emitTLSError(new ERR_TLS_HANDSHAKE_TIMEOUT());
};

TLSSocket.prototype._emitTLSError = function(err) {
  var e = this._tlsError(err);
  if (e)
    this.emit('error', e);
};

TLSSocket.prototype._tlsError = function(err) {
  this.emit('_tlsError', err);
  if (this._controlReleased)
    return err;
  return null;
};

TLSSocket.prototype._releaseControl = function() {
  if (this._controlReleased)
    return false;
  this._controlReleased = true;
  this.removeListener('error', this._tlsError);
  return true;
};

TLSSocket.prototype._finishInit = function() {
  debug('secure established');
  this.alpnProtocol = this._handle.getALPNNegotiatedProtocol();
  this.servername = this._handle.getServername();
  this._secureEstablished = true;
  if (this._tlsOptions.handshakeTimeout > 0)
    this.setTimeout(0, this._handleTimeout);
  this.emit('secure');
};

TLSSocket.prototype._start = function() {
  if (this.connecting) {
    this.once('connect', this._start);
    return;
  }

  // Socket was destroyed before the connection was established
  if (!this._handle)
    return;

  debug('start');
  if (this._tlsOptions.requestOCSP)
    this._handle.requestOCSP();
  this._handle.start();
};

TLSSocket.prototype.setServername = function(name) {
  validateString(name, 'name');

  if (this._tlsOptions.isServer) {
    throw new ERR_TLS_SNI_FROM_SERVER();
  }

  this._handle.setServername(name);
};

TLSSocket.prototype.setSession = function(session) {
  if (typeof session === 'string')
    session = Buffer.from(session, 'latin1');
  this._handle.setSession(session);
};

TLSSocket.prototype.getPeerCertificate = function(detailed) {
  if (this._handle) {
    return common.translatePeerCertificate(
      this._handle.getPeerCertificate(detailed)) || {};
  }

  return null;
};

TLSSocket.prototype.getCertificate = function() {
  if (this._handle) {
    // It's not a peer cert, but the formatting is identical.
    return common.translatePeerCertificate(
      this._handle.getCertificate()) || {};
  }

  return null;
};

// Proxy TLSSocket handle methods
function makeSocketMethodProxy(name) {
  return function socketMethodProxy(...args) {
    if (this._handle)
      return this._handle[name].apply(this._handle, args);
    return null;
  };
}

[
  'getFinished', 'getPeerFinished', 'getSession', 'isSessionReused',
  'getEphemeralKeyInfo', 'getProtocol', 'getTLSTicket'
].forEach((method) => {
  TLSSocket.prototype[method] = makeSocketMethodProxy(method);
});

TLSSocket.prototype.getCipher = function(err) {
  if (this._handle)
    return this._handle.getCurrentCipher();
  return null;
};

// TODO: support anonymous (nocert) and PSK


function onServerSocketSecure() {
  if (this._requestCert) {
    const verifyError = this._handle.verifyError();
    if (verifyError) {
      this.authorizationError = verifyError.code;

      if (this._rejectUnauthorized)
        this.destroy();
    } else {
      this.authorized = true;
    }
  }

  if (!this.destroyed && this._releaseControl())
    this._tlsOptions.server.emit('secureConnection', this);
}

function onSocketTLSError(err) {
  if (!this._controlReleased && !this[kErrorEmitted]) {
    this[kErrorEmitted] = true;
    this._tlsOptions.server.emit('tlsClientError', err, this);
  }
}

function onSocketClose(err) {
  // Closed because of error - no need to emit it twice
  if (err)
    return;

  // Emit ECONNRESET
  if (!this._controlReleased && !this[kErrorEmitted]) {
    this[kErrorEmitted] = true;
    // eslint-disable-next-line no-restricted-syntax
    const connReset = new Error('socket hang up');
    connReset.code = 'ECONNRESET';
    this._tlsOptions.server.emit('tlsClientError', connReset, this);
  }
}

function tlsConnectionListener(rawSocket) {
  const socket = new TLSSocket(rawSocket, {
    secureContext: this._sharedCreds,
    isServer: true,
    server: this,
    requestCert: this.requestCert,
    rejectUnauthorized: this.rejectUnauthorized,
    handshakeTimeout: this[kHandshakeTimeout],
    ALPNProtocols: this.ALPNProtocols,
    SNICallback: this[kSNICallback] || SNICallback
  });

  socket.on('secure', onServerSocketSecure);

  socket[kErrorEmitted] = false;
  socket.on('close', onSocketClose);
  socket.on('_tlsError', onSocketTLSError);
}

// AUTHENTICATION MODES
//
// There are several levels of authentication that TLS/SSL supports.
// Read more about this in "man SSL_set_verify".
//
// 1. The server sends a certificate to the client but does not request a
// cert from the client. This is common for most HTTPS servers. The browser
// can verify the identity of the server, but the server does not know who
// the client is. Authenticating the client is usually done over HTTP using
// login boxes and cookies and stuff.
//
// 2. The server sends a cert to the client and requests that the client
// also send it a cert. The client knows who the server is and the server is
// requesting the client also identify themselves. There are several
// outcomes:
//
//   A) verifyError returns null meaning the client's certificate is signed
//   by one of the server's CAs. The server now knows the client's identity
//   and the client is authorized.
//
//   B) For some reason the client's certificate is not acceptable -
//   verifyError returns a string indicating the problem. The server can
//   either (i) reject the client or (ii) allow the client to connect as an
//   unauthorized connection.
//
// The mode is controlled by two boolean variables.
//
// requestCert
//   If true the server requests a certificate from client connections. For
//   the common HTTPS case, users will want this to be false, which is what
//   it defaults to.
//
// rejectUnauthorized
//   If true clients whose certificates are invalid for any reason will not
//   be allowed to make connections. If false, they will simply be marked as
//   unauthorized but secure communication will continue. By default this is
//   true.
//
//
//
// Options:
// - requestCert. Send verify request. Default to false.
// - rejectUnauthorized. Boolean, default to true.
// - key. string.
// - cert: string.
// - clientCertEngine: string.
// - ca: string or array of strings.
// - sessionTimeout: integer.
//
// emit 'secureConnection'
//   function (tlsSocket) { }
//
//   "UNABLE_TO_GET_ISSUER_CERT", "UNABLE_TO_GET_CRL",
//   "UNABLE_TO_DECRYPT_CERT_SIGNATURE", "UNABLE_TO_DECRYPT_CRL_SIGNATURE",
//   "UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY", "CERT_SIGNATURE_FAILURE",
//   "CRL_SIGNATURE_FAILURE", "CERT_NOT_YET_VALID" "CERT_HAS_EXPIRED",
//   "CRL_NOT_YET_VALID", "CRL_HAS_EXPIRED" "ERROR_IN_CERT_NOT_BEFORE_FIELD",
//   "ERROR_IN_CERT_NOT_AFTER_FIELD", "ERROR_IN_CRL_LAST_UPDATE_FIELD",
//   "ERROR_IN_CRL_NEXT_UPDATE_FIELD", "OUT_OF_MEM",
//   "DEPTH_ZERO_SELF_SIGNED_CERT", "SELF_SIGNED_CERT_IN_CHAIN",
//   "UNABLE_TO_GET_ISSUER_CERT_LOCALLY", "UNABLE_TO_VERIFY_LEAF_SIGNATURE",
//   "CERT_CHAIN_TOO_LONG", "CERT_REVOKED" "INVALID_CA",
//   "PATH_LENGTH_EXCEEDED", "INVALID_PURPOSE" "CERT_UNTRUSTED",
//   "CERT_REJECTED"
//
function Server(options, listener) {
  if (!(this instanceof Server))
    return new Server(options, listener);

  if (typeof options === 'function') {
    listener = options;
    options = {};
  } else if (options == null || typeof options === 'object') {
    options = options || {};
  } else {
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  }

  this._contexts = [];
  this.requestCert = options.requestCert === true;
  this.rejectUnauthorized = options.rejectUnauthorized !== false;

  if (options.sessionTimeout)
    this.sessionTimeout = options.sessionTimeout;

  if (options.ticketKeys)
    this.ticketKeys = options.ticketKeys;

  if (options.ALPNProtocols)
    tls.convertALPNProtocols(options.ALPNProtocols, this);

  this.setSecureContext(options);

  this[kHandshakeTimeout] = options.handshakeTimeout || (120 * 1000);
  this[kSNICallback] = options.SNICallback;

  if (typeof this[kHandshakeTimeout] !== 'number') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.handshakeTimeout', 'number', options.handshakeTimeout);
  }

  if (this[kSNICallback] && typeof this[kSNICallback] !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.SNICallback', 'function', options.SNICallback);
  }

  // constructor call
  net.Server.call(this, tlsConnectionListener);

  if (listener) {
    this.on('secureConnection', listener);
  }
}

util.inherits(Server, net.Server);
exports.Server = Server;
exports.createServer = function createServer(options, listener) {
  return new Server(options, listener);
};


Server.prototype.setSecureContext = function(options) {
  if (options === null || typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);

  if (options.pfx)
    this.pfx = options.pfx;
  else
    this.pfx = undefined;

  if (options.key)
    this.key = options.key;
  else
    this.key = undefined;

  if (options.passphrase)
    this.passphrase = options.passphrase;
  else
    this.passphrase = undefined;

  if (options.cert)
    this.cert = options.cert;
  else
    this.cert = undefined;

  if (options.clientCertEngine)
    this.clientCertEngine = options.clientCertEngine;
  else
    this.clientCertEngine = undefined;

  if (options.ca)
    this.ca = options.ca;
  else
    this.ca = undefined;

  if (options.minVersion)
    this.minVersion = options.minVersion;
  else
    this.minVersion = undefined;

  if (options.maxVersion)
    this.maxVersion = options.maxVersion;
  else
    this.maxVersion = undefined;

  if (options.secureProtocol)
    this.secureProtocol = options.secureProtocol;
  else
    this.secureProtocol = undefined;

  if (options.crl)
    this.crl = options.crl;
  else
    this.crl = undefined;

  if (options.ciphers)
    this.ciphers = options.ciphers;
  else
    this.ciphers = undefined;

  if (options.ecdhCurve !== undefined)
    this.ecdhCurve = options.ecdhCurve;
  else
    this.ecdhCurve = undefined;

  if (options.dhparam)
    this.dhparam = options.dhparam;
  else
    this.dhparam = undefined;

  if (options.honorCipherOrder !== undefined)
    this.honorCipherOrder = !!options.honorCipherOrder;
  else
    this.honorCipherOrder = true;

  const secureOptions = options.secureOptions || 0;

  if (secureOptions)
    this.secureOptions = secureOptions;
  else
    this.secureOptions = undefined;

  if (options.sessionIdContext) {
    this.sessionIdContext = options.sessionIdContext;
  } else {
    this.sessionIdContext = crypto.createHash('sha1')
                                  .update(process.argv.join(' '))
                                  .digest('hex')
                                  .slice(0, 32);
  }

  this._sharedCreds = tls.createSecureContext({
    pfx: this.pfx,
    key: this.key,
    passphrase: this.passphrase,
    cert: this.cert,
    clientCertEngine: this.clientCertEngine,
    ca: this.ca,
    ciphers: this.ciphers,
    ecdhCurve: this.ecdhCurve,
    dhparam: this.dhparam,
    minVersion: this.minVersion,
    maxVersion: this.maxVersion,
    secureProtocol: this.secureProtocol,
    secureOptions: this.secureOptions,
    honorCipherOrder: this.honorCipherOrder,
    crl: this.crl,
    sessionIdContext: this.sessionIdContext
  });

  if (this.sessionTimeout)
    this._sharedCreds.context.setSessionTimeout(this.sessionTimeout);

  if (options.ticketKeys) {
    this.ticketKeys = options.ticketKeys;
    this.setTicketKeys(this.ticketKeys);
  }
};


Server.prototype._getServerData = function() {
  return {
    ticketKeys: this.getTicketKeys().toString('hex')
  };
};


Server.prototype._setServerData = function(data) {
  this.setTicketKeys(Buffer.from(data.ticketKeys, 'hex'));
};


Server.prototype.getTicketKeys = function getTicketKeys() {
  return this._sharedCreds.context.getTicketKeys();
};


Server.prototype.setTicketKeys = function setTicketKeys(keys) {
  this._sharedCreds.context.setTicketKeys(keys);
};


Server.prototype.setOptions = util.deprecate(function(options) {
  this.requestCert = options.requestCert === true;
  this.rejectUnauthorized = options.rejectUnauthorized !== false;

  if (options.pfx) this.pfx = options.pfx;
  if (options.key) this.key = options.key;
  if (options.passphrase) this.passphrase = options.passphrase;
  if (options.cert) this.cert = options.cert;
  if (options.clientCertEngine)
    this.clientCertEngine = options.clientCertEngine;
  if (options.ca) this.ca = options.ca;
  if (options.minVersion) this.minVersion = options.minVersion;
  if (options.maxVersion) this.maxVersion = options.maxVersion;
  if (options.secureProtocol) this.secureProtocol = options.secureProtocol;
  if (options.crl) this.crl = options.crl;
  if (options.ciphers) this.ciphers = options.ciphers;
  if (options.ecdhCurve !== undefined)
    this.ecdhCurve = options.ecdhCurve;
  if (options.dhparam) this.dhparam = options.dhparam;
  if (options.sessionTimeout) this.sessionTimeout = options.sessionTimeout;
  if (options.ticketKeys) this.ticketKeys = options.ticketKeys;
  var secureOptions = options.secureOptions || 0;
  if (options.honorCipherOrder !== undefined)
    this.honorCipherOrder = !!options.honorCipherOrder;
  else
    this.honorCipherOrder = true;
  if (secureOptions) this.secureOptions = secureOptions;
  if (options.ALPNProtocols)
    tls.convertALPNProtocols(options.ALPNProtocols, this);
  if (options.sessionIdContext) {
    this.sessionIdContext = options.sessionIdContext;
  } else {
    this.sessionIdContext = crypto.createHash('sha1')
                                  .update(process.argv.join(' '))
                                  .digest('hex')
                                  .slice(0, 32);
  }
}, 'Server.prototype.setOptions() is deprecated', 'DEP0122');

// SNI Contexts High-Level API
Server.prototype.addContext = function(servername, context) {
  if (!servername) {
    throw new ERR_TLS_REQUIRED_SERVER_NAME();
  }

  var re = new RegExp('^' +
                      servername.replace(/([.^$+?\-\\[\]{}])/g, '\\$1')
                                .replace(/\*/g, '[^.]*') +
                      '$');
  this._contexts.push([re, tls.createSecureContext(context).context]);
};

function SNICallback(servername, callback) {
  const contexts = this.server._contexts;

  for (var i = 0; i < contexts.length; i++) {
    const elem = contexts[i];
    if (elem[0].test(servername)) {
      callback(null, elem[1]);
      return;
    }
  }

  callback(null, undefined);
}


// Target API:
//
//  var s = tls.connect({port: 8000, host: "google.com"}, function() {
//    if (!s.authorized) {
//      s.destroy();
//      return;
//    }
//
//    // s.socket;
//
//    s.end("hello world\n");
//  });
//
//
function normalizeConnectArgs(listArgs) {
  const args = net._normalizeArgs(listArgs);
  const options = args[0];
  const cb = args[1];

  // If args[0] was options, then normalize dealt with it.
  // If args[0] is port, or args[0], args[1] is host, port, we need to
  // find the options and merge them in, normalize's options has only
  // the host/port/path args that it knows about, not the tls options.
  // This means that options.host overrides a host arg.
  if (listArgs[1] !== null && typeof listArgs[1] === 'object') {
    Object.assign(options, listArgs[1]);
  } else if (listArgs[2] !== null && typeof listArgs[2] === 'object') {
    Object.assign(options, listArgs[2]);
  }

  return cb ? [options, cb] : [options];
}

function onConnectSecure() {
  const options = this[kConnectOptions];

  // Check the size of DHE parameter above minimum requirement
  // specified in options.
  const ekeyinfo = this.getEphemeralKeyInfo();
  if (ekeyinfo.type === 'DH' && ekeyinfo.size < options.minDHSize) {
    const err = new ERR_TLS_DH_PARAM_SIZE(ekeyinfo.size);
    this.emit('error', err);
    this.destroy();
    return;
  }

  let verifyError = this._handle.verifyError();

  // Verify that server's identity matches it's certificate's names
  // Unless server has resumed our existing session
  if (!verifyError && !this.isSessionReused()) {
    const hostname = options.servername ||
                   options.host ||
                   (options.socket && options.socket._host) ||
                   'localhost';
    const cert = this.getPeerCertificate(true);
    verifyError = options.checkServerIdentity(hostname, cert);
  }

  if (verifyError) {
    this.authorized = false;
    this.authorizationError = verifyError.code || verifyError.message;

    if (options.rejectUnauthorized) {
      this.destroy(verifyError);
      return;
    } else {
      this.emit('secureConnect');
    }
  } else {
    this.authorized = true;
    this.emit('secureConnect');
  }

  this.removeListener('end', onConnectEnd);
}

function onConnectEnd() {
  // NOTE: This logic is shared with _http_client.js
  if (!this._hadError) {
    const options = this[kConnectOptions];
    this._hadError = true;
    // eslint-disable-next-line no-restricted-syntax
    const error = new Error('Client network socket disconnected before ' +
                            'secure TLS connection was established');
    error.code = 'ECONNRESET';
    error.path = options.path;
    error.host = options.host;
    error.port = options.port;
    error.localAddress = options.localAddress;
    this.destroy(error);
  }
}

let warnOnAllowUnauthorized = true;

// Arguments: [port,] [host,] [options,] [cb]
exports.connect = function connect(...args) {
  args = normalizeConnectArgs(args);
  var options = args[0];
  var cb = args[1];
  const allowUnauthorized = process.env.NODE_TLS_REJECT_UNAUTHORIZED === '0';

  if (allowUnauthorized && warnOnAllowUnauthorized) {
    warnOnAllowUnauthorized = false;
    process.emitWarning('Setting the NODE_TLS_REJECT_UNAUTHORIZED ' +
                        'environment variable to \'0\' makes TLS connections ' +
                        'and HTTPS requests insecure by disabling ' +
                        'certificate verification.');
  }

  options = {
    rejectUnauthorized: !allowUnauthorized,
    ciphers: tls.DEFAULT_CIPHERS,
    checkServerIdentity: tls.checkServerIdentity,
    minDHSize: 1024,
    ...options
  };

  if (!options.keepAlive)
    options.singleUse = true;

  assert(typeof options.checkServerIdentity === 'function');
  assert(typeof options.minDHSize === 'number',
         'options.minDHSize is not a number: ' + options.minDHSize);
  assert(options.minDHSize > 0,
         'options.minDHSize is not a positive number: ' +
         options.minDHSize);

  const context = options.secureContext || tls.createSecureContext(options);

  var tlssock = new TLSSocket(options.socket, {
    pipe: !!options.path,
    secureContext: context,
    isServer: false,
    requestCert: true,
    rejectUnauthorized: options.rejectUnauthorized !== false,
    session: options.session,
    ALPNProtocols: options.ALPNProtocols,
    requestOCSP: options.requestOCSP
  });

  tlssock[kConnectOptions] = options;

  if (cb)
    tlssock.once('secureConnect', cb);

  if (!options.socket) {
    // If user provided the socket, its their responsibility to manage its
    // connectivity. If we created one internally, we connect it.
    const connectOpt = {
      path: options.path,
      port: options.port,
      host: options.host,
      family: options.family,
      localAddress: options.localAddress,
      lookup: options.lookup
    };

    if (options.timeout) {
      tlssock.setTimeout(options.timeout);
    }

    tlssock.connect(connectOpt, tlssock._start);
  }

  tlssock._releaseControl();

  if (options.session)
    tlssock.setSession(options.session);

  if (options.servername) {
    if (!ipServernameWarned && net.isIP(options.servername)) {
      process.emitWarning(
        'Setting the TLS ServerName to an IP address is not permitted by ' +
        'RFC 6066. This will be ignored in a future version.',
        'DeprecationWarning',
        'DEP0123'
      );
      ipServernameWarned = true;
    }
    tlssock.setServername(options.servername);
  }

  if (options.socket)
    tlssock._start();

  tlssock.on('secure', onConnectSecure);
  tlssock.once('end', onConnectEnd);

  return tlssock;
};
