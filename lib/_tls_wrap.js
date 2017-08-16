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

const assert = require('assert');
const crypto = require('crypto');
const net = require('net');
const tls = require('tls');
const util = require('util');
const common = require('_tls_common');
const StreamWrap = require('_stream_wrap').StreamWrap;
const Buffer = require('buffer').Buffer;
const debug = util.debuglog('tls');
const Timer = process.binding('timer_wrap').Timer;
const tls_wrap = process.binding('tls_wrap');
const TCP = process.binding('tcp_wrap').TCP;
const Pipe = process.binding('pipe_wrap').Pipe;
const errors = require('internal/errors');
const kDisableRenegotiation = Symbol('disable-renegotiation');

function onhandshakestart() {
  debug('onhandshakestart');

  var self = this;
  var ssl = self._handle;
  var now = Timer.now();

  assert(now >= ssl.lastHandshakeTime);

  if ((now - ssl.lastHandshakeTime) >= tls.CLIENT_RENEG_WINDOW * 1000) {
    ssl.handshakes = 0;
  }

  var first = (ssl.lastHandshakeTime === 0);
  ssl.lastHandshakeTime = now;
  if (first) return;

  if (++ssl.handshakes > tls.CLIENT_RENEG_LIMIT) {
    // Defer the error event to the next tick. We're being called from OpenSSL's
    // state machine and OpenSSL is not re-entrant. We cannot allow the user's
    // callback to destroy the connection right now, it would crash and burn.
    setImmediate(function() {
      var err = new errors.Error('ERR_TLS_SESSION_ATTACK');
      self._emitTLSError(err);
    });
  }

  if (this[kDisableRenegotiation] && ssl.handshakes > 0) {
    const err = new Error('TLS session renegotiation disabled for this socket');
    self._emitTLSError(err);
  }
}


function onhandshakedone() {
  // for future use
  debug('onhandshakedone');
  this._finishInit();
}


function loadSession(self, hello, cb) {
  var once = false;
  function onSession(err, session) {
    if (once)
      return cb(new errors.Error('ERR_MULTIPLE_CALLBACK'));
    once = true;

    if (err)
      return cb(err);

    if (!self._handle)
      return cb(new errors.Error('ERR_SOCKET_CLOSED'));

    self._handle.loadSession(session);
    cb(null);
  }

  if (hello.sessionId.length <= 0 ||
      hello.tlsTicket ||
      self.server &&
      !self.server.emit('resumeSession', hello.sessionId, onSession)) {
    cb(null);
  }
}


function loadSNI(self, servername, cb) {
  if (!servername || !self._SNICallback)
    return cb(null);

  var once = false;
  self._SNICallback(servername, function(err, context) {
    if (once)
      return cb(new errors.Error('ERR_MULTIPLE_CALLBACK'));
    once = true;

    if (err)
      return cb(err);

    if (!self._handle)
      return cb(new errors.Error('ERR_SOCKET_CLOSED'));

    // TODO(indutny): eventually disallow raw `SecureContext`
    if (context)
      self._handle.sni_context = context.context || context;

    cb(null, self._handle.sni_context);
  });
}


function requestOCSP(self, hello, ctx, cb) {
  if (!hello.OCSPRequest || !self.server)
    return cb(null);

  if (!ctx)
    ctx = self.server._sharedCreds;

  // TLS socket is using a `net.Server` instead of a tls.TLSServer.
  // Some TLS properties like `server._sharedCreds` will not be present
  if (!ctx)
    return cb(null);

  // TODO(indutny): eventually disallow raw `SecureContext`
  if (ctx.context)
    ctx = ctx.context;

  if (self.server.listenerCount('OCSPRequest') === 0) {
    return cb(null);
  } else {
    self.server.emit('OCSPRequest',
                     ctx.getCertificate(),
                     ctx.getIssuer(),
                     onOCSP);
  }

  var once = false;
  function onOCSP(err, response) {
    if (once)
      return cb(new errors.Error('ERR_MULTIPLE_CALLBACK'));
    once = true;

    if (err)
      return cb(err);

    if (!self._handle)
      return cb(new errors.Error('ERR_SOCKET_CLOSED'));

    if (response)
      self._handle.setOCSPResponse(response);
    cb(null);
  }
}


function onclienthello(hello) {
  var self = this;

  loadSession(self, hello, function(err) {
    if (err)
      return self.destroy(err);

    self._handle.endParser();
  });
}


function oncertcb(info) {
  var self = this;
  var servername = info.servername;

  loadSNI(self, servername, function(err, ctx) {
    if (err)
      return self.destroy(err);
    requestOCSP(self, info, ctx, function(err) {
      if (err)
        return self.destroy(err);

      if (!self._handle)
        return self.destroy(new errors.Error('ERR_SOCKET_CLOSED'));

      try {
        self._handle.certCbDone();
      } catch (e) {
        self.destroy(e);
      }
    });
  });
}


function onnewsession(key, session) {
  if (!this.server)
    return;

  var self = this;
  var once = false;

  this._newSessionPending = true;
  if (!this.server.emit('newSession', key, session, done))
    done();

  function done() {
    if (once)
      return;
    once = true;

    if (!self._handle)
      return self.destroy(new errors.Error('ERR_SOCKET_CLOSED'));

    self._handle.newSessionDone();

    self._newSessionPending = false;
    if (self._securePending)
      self._finishInit();
    self._securePending = false;
  }
}


function onocspresponse(resp) {
  this.emit('OCSPResponse', resp);
}

function initRead(tls, wrapped) {
  // If we were destroyed already don't bother reading
  if (!tls._handle)
    return;

  // Socket already has some buffered data - emulate receiving it
  if (wrapped && wrapped._readableState && wrapped._readableState.length) {
    var buf;
    while ((buf = wrapped.read()) !== null)
      tls._handle.receive(buf);
  }

  tls.read(0);
}

/**
 * Provides a wrap of socket stream to do encrypted communication.
 */

function TLSSocket(socket, options) {
  if (options === undefined)
    this._tlsOptions = {};
  else
    this._tlsOptions = options;
  this._secureEstablished = false;
  this._securePending = false;
  this._newSessionPending = false;
  this._controlReleased = false;
  this._SNICallback = null;
  this.servername = null;
  this.npnProtocol = null;
  this.alpnProtocol = null;
  this.authorized = false;
  this.authorizationError = null;

  // Wrap plain JS Stream into StreamWrap
  var wrap;
  if ((socket instanceof net.Socket && socket._handle) || !socket)
    wrap = socket;
  else
    wrap = new StreamWrap(socket);

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
  this.ssl = this._handle;

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
  if (this.owner) {
    ssl = this.owner.ssl;
    this.owner.ssl = null;
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
  var res;
  var handle;

  if (wrap)
    handle = wrap._handle;

  var options = this._tlsOptions;
  if (!handle) {
    handle = options.pipe ? new Pipe() : new TCP();
    handle.owner = this;
  }

  // Wrap socket's handle
  var context = options.secureContext ||
                options.credentials ||
                tls.createSecureContext(options);
  res = tls_wrap.wrap(handle._externalStream,
                      context.context,
                      !!options.isServer);
  res._parent = handle;
  res._parentWrap = wrap;
  res._secureContext = context;
  res.reading = handle.reading;
  Object.defineProperty(handle, 'reading', {
    get: function get() {
      return res.reading;
    },
    set: function set(value) {
      res.reading = value;
    }
  });

  this.on('close', function() {
    // Make sure we are not doing it on OpenSSL's stack
    setImmediate(destroySSL, this);
    res = null;
  });

  return res;
};

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
  var self = this;
  var options = this._tlsOptions;
  var ssl = this._handle;

  // lib/net.js expect this value to be non-zero if write hasn't been flushed
  // immediately
  // TODO(indutny): revise this solution, it might be 1 before handshake and
  // represent real writeQueueSize during regular writes.
  ssl.writeQueueSize = 1;

  this.server = options.server;

  // For clients, we will always have either a given ca list or be using
  // default one
  const requestCert = !!options.requestCert || !options.isServer;
  const rejectUnauthorized = !!options.rejectUnauthorized;

  this._requestCert = requestCert;
  this._rejectUnauthorized = rejectUnauthorized;
  if (requestCert || rejectUnauthorized)
    ssl.setVerifyMode(requestCert, rejectUnauthorized);

  if (options.isServer) {
    ssl.onhandshakestart = () => onhandshakestart.call(this);
    ssl.onhandshakedone = () => onhandshakedone.call(this);
    ssl.onclienthello = (hello) => onclienthello.call(this, hello);
    ssl.oncertcb = (info) => oncertcb.call(this, info);
    ssl.onnewsession = (key, session) => onnewsession.call(this, key, session);
    ssl.lastHandshakeTime = 0;
    ssl.handshakes = 0;

    if (this.server) {
      if (this.server.listenerCount('resumeSession') > 0 ||
          this.server.listenerCount('newSession') > 0) {
        ssl.enableSessionCallbacks();
      }
      if (this.server.listenerCount('OCSPRequest') > 0)
        ssl.enableCertCb();
    }
  } else {
    ssl.onhandshakestart = function() {};
    ssl.onhandshakedone = () => this._finishInit();
    ssl.onocspresponse = (resp) => onocspresponse.call(this, resp);

    if (options.session)
      ssl.setSession(options.session);
  }

  ssl.onerror = function(err) {
    if (self._writableState.errorEmitted)
      return;

    // Destroy socket if error happened before handshake's finish
    if (!self._secureEstablished) {
      // When handshake fails control is not yet released,
      // so self._tlsError will return null instead of actual error
      self.destroy(err);
    } else if (options.isServer &&
               rejectUnauthorized &&
               /peer did not return a certificate/.test(err.message)) {
      // Ignore server's authorization errors
      self.destroy();
    } else {
      // Throw error
      self._emitTLSError(err);
    }

    self._writableState.errorEmitted = true;
  };

  // If custom SNICallback was given, or if
  // there're SNI contexts to perform match against -
  // set `.onsniselect` callback.
  if (process.features.tls_sni &&
      options.isServer &&
      options.SNICallback &&
      options.server &&
      (options.SNICallback !== SNICallback ||
       options.server._contexts.length)) {
    assert(typeof options.SNICallback === 'function');
    this._SNICallback = options.SNICallback;
    ssl.enableCertCb();
  }

  if (process.features.tls_npn && options.NPNProtocols)
    ssl.setNPNProtocols(options.NPNProtocols);

  if (process.features.tls_alpn && options.ALPNProtocols) {
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
    socket.once('connect', function() {
      self.connecting = false;
      self.emit('connect');
    });
  }

  // Assume `tls.connect()`
  if (wrap) {
    wrap.on('error', function(err) {
      self._emitTLSError(err);
    });
  } else {
    assert(!socket);
    this.connecting = true;
  }
};

TLSSocket.prototype.renegotiate = function(options, callback) {
  var requestCert = this._requestCert;
  var rejectUnauthorized = this._rejectUnauthorized;

  if (this.destroyed)
    return;

  if (typeof options.requestCert !== 'undefined')
    requestCert = !!options.requestCert;
  if (typeof options.rejectUnauthorized !== 'undefined')
    rejectUnauthorized = !!options.rejectUnauthorized;

  if (requestCert !== this._requestCert ||
      rejectUnauthorized !== this._rejectUnauthorized) {
    this._handle.setVerifyMode(requestCert, rejectUnauthorized);
    this._requestCert = requestCert;
    this._rejectUnauthorized = rejectUnauthorized;
  }
  if (!this._handle.renegotiate()) {
    if (callback) {
      process.nextTick(callback, new errors.Error('ERR_TLS_RENEGOTIATE'));
    }
    return false;
  }

  // Ensure that we'll cycle through internal openssl's state
  this.write('');

  if (callback) {
    this.once('secure', function() {
      callback(null);
    });
  }

  return true;
};

TLSSocket.prototype.setMaxSendFragment = function setMaxSendFragment(size) {
  return this._handle.setMaxSendFragment(size) === 1;
};

TLSSocket.prototype.getTLSTicket = function getTLSTicket() {
  return this._handle.getTLSTicket();
};

TLSSocket.prototype._handleTimeout = function() {
  this._emitTLSError(new errors.Error('ERR_TLS_HANDSHAKE_TIMEOUT'));
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
  // `newSession` callback wasn't called yet
  if (this._newSessionPending) {
    this._securePending = true;
    return;
  }

  if (process.features.tls_npn) {
    this.npnProtocol = this._handle.getNegotiatedProtocol();
  }

  if (process.features.tls_alpn) {
    this.alpnProtocol = this.ssl.getALPNNegotiatedProtocol();
  }

  if (process.features.tls_sni && this._tlsOptions.isServer) {
    this.servername = this._handle.getServername();
  }

  debug('secure established');
  this._secureEstablished = true;
  if (this._tlsOptions.handshakeTimeout > 0)
    this.setTimeout(0, this._handleTimeout);
  this.emit('secure');
};

TLSSocket.prototype._start = function() {
  if (this.connecting) {
    this.once('connect', function() {
      this._start();
    });
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
      this._handle.getPeerCertificate(detailed));
  }

  return null;
};

TLSSocket.prototype.getSession = function() {
  if (this._handle) {
    return this._handle.getSession();
  }

  return null;
};

TLSSocket.prototype.isSessionReused = function() {
  if (this._handle) {
    return this._handle.isSessionReused();
  }

  return null;
};

TLSSocket.prototype.getCipher = function(err) {
  if (this._handle) {
    return this._handle.getCurrentCipher();
  } else {
    return null;
  }
};

TLSSocket.prototype.getEphemeralKeyInfo = function() {
  if (this._handle)
    return this._handle.getEphemeralKeyInfo();

  return null;
};

TLSSocket.prototype.getProtocol = function() {
  if (this._handle)
    return this._handle.getProtocol();

  return null;
};

// TODO: support anonymous (nocert) and PSK


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
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options', 'object');
  }


  this._contexts = [];

  var self = this;

  // Handle option defaults:
  this.setOptions(options);

  var sharedCreds = tls.createSecureContext({
    pfx: self.pfx,
    key: self.key,
    passphrase: self.passphrase,
    cert: self.cert,
    ca: self.ca,
    ciphers: self.ciphers,
    ecdhCurve: self.ecdhCurve,
    dhparam: self.dhparam,
    secureProtocol: self.secureProtocol,
    secureOptions: self.secureOptions,
    honorCipherOrder: self.honorCipherOrder,
    crl: self.crl,
    sessionIdContext: self.sessionIdContext
  });
  this._sharedCreds = sharedCreds;

  var timeout = options.handshakeTimeout || (120 * 1000);

  if (typeof timeout !== 'number') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'timeout', 'number');
  }

  if (self.sessionTimeout) {
    sharedCreds.context.setSessionTimeout(self.sessionTimeout);
  }

  if (self.ticketKeys) {
    sharedCreds.context.setTicketKeys(self.ticketKeys);
  }

  // constructor call
  net.Server.call(this, function(raw_socket) {
    var socket = new TLSSocket(raw_socket, {
      secureContext: sharedCreds,
      isServer: true,
      server: self,
      requestCert: self.requestCert,
      rejectUnauthorized: self.rejectUnauthorized,
      handshakeTimeout: timeout,
      NPNProtocols: self.NPNProtocols,
      ALPNProtocols: self.ALPNProtocols,
      SNICallback: options.SNICallback || SNICallback
    });

    socket.on('secure', function() {
      if (socket._requestCert) {
        var verifyError = socket._handle.verifyError();
        if (verifyError) {
          socket.authorizationError = verifyError.code;

          if (socket._rejectUnauthorized)
            socket.destroy();
        } else {
          socket.authorized = true;
        }
      }

      if (!socket.destroyed && socket._releaseControl())
        self.emit('secureConnection', socket);
    });

    var errorEmitted = false;
    socket.on('close', function(err) {
      // Closed because of error - no need to emit it twice
      if (err)
        return;

      // Emit ECONNRESET
      if (!socket._controlReleased && !errorEmitted) {
        errorEmitted = true;
        var connReset = new Error('socket hang up');
        connReset.code = 'ECONNRESET';
        self.emit('tlsClientError', connReset, socket);
      }
    });

    socket.on('_tlsError', function(err) {
      if (!socket._controlReleased && !errorEmitted) {
        errorEmitted = true;
        self.emit('tlsClientError', err, socket);
      }
    });
  });

  if (listener) {
    this.on('secureConnection', listener);
  }
}

util.inherits(Server, net.Server);
exports.Server = Server;
exports.createServer = function(options, listener) {
  return new Server(options, listener);
};


Server.prototype._getServerData = function() {
  return {
    ticketKeys: this.getTicketKeys().toString('hex')
  };
};


Server.prototype._setServerData = function(data) {
  this.setTicketKeys(Buffer.from(data.ticketKeys, 'hex'));
};


Server.prototype.getTicketKeys = function getTicketKeys(keys) {
  return this._sharedCreds.context.getTicketKeys(keys);
};


Server.prototype.setTicketKeys = function setTicketKeys(keys) {
  this._sharedCreds.context.setTicketKeys(keys);
};


Server.prototype.setOptions = function(options) {
  this.requestCert = options.requestCert === true;
  this.rejectUnauthorized = options.rejectUnauthorized !== false;

  if (options.pfx) this.pfx = options.pfx;
  if (options.key) this.key = options.key;
  if (options.passphrase) this.passphrase = options.passphrase;
  if (options.cert) this.cert = options.cert;
  if (options.ca) this.ca = options.ca;
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
  if (options.NPNProtocols) tls.convertNPNProtocols(options.NPNProtocols, this);
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
};

// SNI Contexts High-Level API
Server.prototype.addContext = function(servername, context) {
  if (!servername) {
    throw new errors.Error('ERR_TLS_REQUIRED_SERVER_NAME');
  }

  var re = new RegExp('^' +
                      servername.replace(/([.^$+?\-\\[\]{}])/g, '\\$1')
                                .replace(/\*/g, '[^.]*') +
                      '$');
  this._contexts.push([re, tls.createSecureContext(context).context]);
};

function SNICallback(servername, callback) {
  var ctx;

  this.server._contexts.some(function(elem) {
    if (elem[0].test(servername)) {
      ctx = elem[1];
      return true;
    }
  });

  callback(null, ctx);
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
  var args = net._normalizeArgs(listArgs);
  var options = args[0];
  var cb = args[1];

  // If args[0] was options, then normalize dealt with it.
  // If args[0] is port, or args[0], args[1] is host, port, we need to
  // find the options and merge them in, normalize's options has only
  // the host/port/path args that it knows about, not the tls options.
  // This means that options.host overrides a host arg.
  if (listArgs[1] !== null && typeof listArgs[1] === 'object') {
    util._extend(options, listArgs[1]);
  } else if (listArgs[2] !== null && typeof listArgs[2] === 'object') {
    util._extend(options, listArgs[2]);
  }

  return (cb) ? [options, cb] : [options];
}

exports.connect = function(...args /* [port,] [host,] [options,] [cb] */) {
  args = normalizeConnectArgs(args);
  var options = args[0];
  var cb = args[1];

  var defaults = {
    rejectUnauthorized: '0' !== process.env.NODE_TLS_REJECT_UNAUTHORIZED,
    ciphers: tls.DEFAULT_CIPHERS,
    checkServerIdentity: tls.checkServerIdentity,
    minDHSize: 1024
  };

  options = util._extend(defaults, options || {});
  if (!options.keepAlive)
    options.singleUse = true;

  assert(typeof options.checkServerIdentity === 'function');
  assert(typeof options.minDHSize === 'number',
         'options.minDHSize is not a number: ' + options.minDHSize);
  assert(options.minDHSize > 0,
         'options.minDHSize is not a positive number: ' +
         options.minDHSize);

  var hostname = options.servername ||
                 options.host ||
                 (options.socket && options.socket._host) ||
                 'localhost';
  const NPN = {};
  const ALPN = {};
  const context = options.secureContext || tls.createSecureContext(options);
  tls.convertNPNProtocols(options.NPNProtocols, NPN);
  tls.convertALPNProtocols(options.ALPNProtocols, ALPN);

  var socket = new TLSSocket(options.socket, {
    pipe: options.path && !options.port,
    secureContext: context,
    isServer: false,
    requestCert: true,
    rejectUnauthorized: options.rejectUnauthorized !== false,
    session: options.session,
    NPNProtocols: NPN.NPNProtocols,
    ALPNProtocols: ALPN.ALPNProtocols,
    requestOCSP: options.requestOCSP
  });

  if (cb)
    socket.once('secureConnect', cb);

  if (!options.socket) {
    var connect_opt;
    if (options.path && !options.port) {
      connect_opt = { path: options.path };
    } else {
      connect_opt = {
        port: options.port,
        host: options.host,
        family: options.family,
        localAddress: options.localAddress,
        lookup: options.lookup
      };
    }
    socket.connect(connect_opt, function() {
      socket._start();
    });
  }

  socket._releaseControl();

  if (options.session)
    socket.setSession(options.session);

  if (options.servername)
    socket.setServername(options.servername);

  if (options.socket)
    socket._start();

  socket.on('secure', function() {
    // Check the size of DHE parameter above minimum requirement
    // specified in options.
    var ekeyinfo = socket.getEphemeralKeyInfo();
    if (ekeyinfo.type === 'DH' && ekeyinfo.size < options.minDHSize) {
      var err = new errors.Error('ERR_TLS_DH_PARAM_SIZE', ekeyinfo.size);
      socket.emit('error', err);
      socket.destroy();
      return;
    }

    var verifyError = socket._handle.verifyError();

    // Verify that server's identity matches it's certificate's names
    // Unless server has resumed our existing session
    if (!verifyError && !socket.isSessionReused()) {
      var cert = socket.getPeerCertificate();
      verifyError = options.checkServerIdentity(hostname, cert);
    }

    if (verifyError) {
      socket.authorized = false;
      socket.authorizationError = verifyError.code || verifyError.message;

      if (options.rejectUnauthorized) {
        socket.destroy(verifyError);
        return;
      } else {
        socket.emit('secureConnect');
      }
    } else {
      socket.authorized = true;
      socket.emit('secureConnect');
    }

    // Uncork incoming data
    socket.removeListener('end', onHangUp);
  });

  function onHangUp() {
    // NOTE: This logic is shared with _http_client.js
    if (!socket._hadError) {
      socket._hadError = true;
      var error = new Error('socket hang up');
      error.code = 'ECONNRESET';
      error.path = options.path;
      error.host = options.host;
      error.port = options.port;
      error.localAddress = options.localAddress;
      socket.destroy(error);
    }
  }
  socket.once('end', onHangUp);

  return socket;
};
