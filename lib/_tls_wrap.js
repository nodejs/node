'use strict';

const assert = require('assert');
const crypto = require('crypto');
const net = require('net');
const tls = require('tls');
const util = require('util');
const common = require('_tls_common');
const StreamWrap = require('_stream_wrap').StreamWrap;
const Buffer = require('buffer').Buffer;
const Duplex = require('stream').Duplex;
const debug = util.debuglog('tls');
const Timer = process.binding('timer_wrap').Timer;
const tls_wrap = process.binding('tls_wrap');
const TCP = process.binding('tcp_wrap').TCP;
const Pipe = process.binding('pipe_wrap').Pipe;
const defaultSessionIdContext = getDefaultSessionIdContext();

function getDefaultSessionIdContext() {
  var defaultText = process.argv.join(' ');
  /* SSL_MAX_SID_CTX_LENGTH is 128 bits */
  if (process.config.variables.openssl_fips) {
    return crypto.createHash('sha1')
      .update(defaultText)
      .digest('hex').slice(0, 32);
  } else {
    return crypto.createHash('md5')
      .update(defaultText)
      .digest('hex');
  }
}

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
      var err = new Error('TLS session renegotiation attack detected.');
      self._emitTLSError(err);
    });
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
      return cb(new Error('TLS session callback was called 2 times'));
    once = true;

    if (err)
      return cb(err);

    if (!self._handle)
      return cb(new Error('Socket is closed'));

    // NOTE: That we have disabled OpenSSL's internal session storage in
    // `node_crypto.cc` and hence its safe to rely on getting servername only
    // from clienthello or this place.
    var ret = self._handle.loadSession(session);

    cb(null, ret);
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
      return cb(new Error('TLS SNI callback was called 2 times'));
    once = true;

    if (err)
      return cb(err);

    if (!self._handle)
      return cb(new Error('Socket is closed'));

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
      return cb(new Error('TLS OCSP callback was called 2 times'));
    once = true;

    if (err)
      return cb(err);

    if (!self._handle)
      return cb(new Error('Socket is closed'));

    if (response)
      self._handle.setOCSPResponse(response);
    cb(null);
  }
}


function onclienthello(hello) {
  var self = this;

  loadSession(self, hello, function(err, session) {
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
        return self.destroy(new Error('Socket is closed'));

      self._handle.certCbDone();
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
      return self.destroy(new Error('Socket is closed'));

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
  this.authorized = false;
  this.authorizationError = null;

  // Wrap plain JS Stream into StreamWrap
  var wrap;
  if (!(socket instanceof net.Socket) && socket instanceof Duplex)
    wrap = new StreamWrap(socket);
  else if ((socket instanceof net.Socket) && !socket._handle)
    wrap = new StreamWrap(socket);
  else
    wrap = socket;

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

  this.on('error', this._emitTLSError);

  this._init(socket, wrap);

  // Make sure to setup all required properties like: `_connecting` before
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
proxiedMethods.forEach(function(name) {
  tls_wrap.TLSWrap.prototype[name] = function methodProxy() {
    if (this._parent[name])
      return this._parent[name].apply(this._parent, arguments);
  };
});

tls_wrap.TLSWrap.prototype.close = function closeProxy(cb) {
  if (this.owner)
    this.owner.ssl = null;

  if (this._parentWrap && this._parentWrap._handle === this._parent) {
    this._parentWrap.once('close', cb);
    return this._parentWrap.destroy();
  }
  return this._parent.close(cb);
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
                tls.createSecureContext();
  res = tls_wrap.wrap(handle._externalStream,
                      context.context,
                      !!options.isServer);
  res._parent = handle;
  res._parentWrap = wrap;
  res._secureContext = context;
  res.reading = handle.reading;
  Object.defineProperty(handle, 'reading', {
    get: function readingGetter() {
      return res.reading;
    },
    set: function readingSetter(value) {
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
  // TODO(indutny): rewise this solution, it might be 1 before handshake and
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
      self.destroy(self._tlsError(err));
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

  if (options.handshakeTimeout > 0)
    this.setTimeout(options.handshakeTimeout, this._handleTimeout);

  if (socket instanceof net.Socket) {
    this._parent = socket;

    // To prevent assertion in afterConnect() and properly kick off readStart
    this._connecting = socket._connecting || !socket._handle;
    socket.once('connect', function() {
      self._connecting = false;
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
    this._connecting = true;
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
      process.nextTick(callback, new Error('Failed to renegotiate'));
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
  return this._handle.setMaxSendFragment(size) == 1;
};

TLSSocket.prototype.getTLSTicket = function getTLSTicket() {
  return this._handle.getTLSTicket();
};

TLSSocket.prototype._handleTimeout = function() {
  this._emitTLSError(new Error('TLS handshake timeout'));
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
  this.removeListener('error', this._emitTLSError);
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
  if (this._connecting) {
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
    session = new Buffer(session, 'binary');
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
//   by one of the server's CAs. The server know's the client idenity now
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
function Server(/* [options], listener */) {
  var options, listener;

  if (arguments[0] !== null && typeof arguments[0] === 'object') {
    options = arguments[0];
    listener = arguments[1];
  } else if (typeof arguments[0] === 'function') {
    options = {};
    listener = arguments[0];
  }

  if (!(this instanceof Server)) return new Server(options, listener);

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
    throw new TypeError('handshakeTimeout must be a number');
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
        self.emit('clientError', connReset, socket);
      }
    });

    socket.on('_tlsError', function(err) {
      if (!socket._controlReleased && !errorEmitted) {
        errorEmitted = true;
        self.emit('clientError', err, socket);
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
  this.setTicketKeys(new Buffer(data.ticketKeys, 'hex'));
};


Server.prototype.getTicketKeys = function getTicketKeys(keys) {
  return this._sharedCreds.context.getTicketKeys(keys);
};


Server.prototype.setTicketKeys = function setTicketKeys(keys) {
  this._sharedCreds.context.setTicketKeys(keys);
};


Server.prototype.setOptions = function(options) {
  if (typeof options.requestCert === 'boolean') {
    this.requestCert = options.requestCert;
  } else {
    this.requestCert = false;
  }

  if (typeof options.rejectUnauthorized === 'boolean') {
    this.rejectUnauthorized = options.rejectUnauthorized;
  } else {
    this.rejectUnauthorized = false;
  }

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
  if (options.sessionIdContext) {
    this.sessionIdContext = options.sessionIdContext;
  } else {
    this.sessionIdContext = defaultSessionIdContext;
  }
};

// SNI Contexts High-Level API
Server.prototype.addContext = function(servername, context) {
  if (!servername) {
    throw new Error('Servername is required parameter for Server.addContext');
  }

  var re = new RegExp('^' +
                      servername.replace(/([\.^$+?\-\\[\]{}])/g, '\\$1')
                                .replace(/\*/g, '[^\.]*') +
                      '$');
  this._contexts.push([re, tls.createSecureContext(context).context]);
};

function SNICallback(servername, callback) {
  var ctx;

  this.server._contexts.some(function(elem) {
    if (servername.match(elem[0]) !== null) {
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
  var args = net._normalizeConnectArgs(listArgs);
  var options = args[0];
  var cb = args[1];

  if (listArgs[1] !== null && typeof listArgs[1] === 'object') {
    options = util._extend(options, listArgs[1]);
  } else if (listArgs[2] !== null && typeof listArgs[2] === 'object') {
    options = util._extend(options, listArgs[2]);
  }

  return (cb) ? [options, cb] : [options];
}

exports.connect = function(/* [port, host], options, cb */) {
  const argsLen = arguments.length;
  var args = new Array(argsLen);
  for (var i = 0; i < argsLen; i++)
    args[i] = arguments[i];
  args = normalizeConnectArgs(args);
  var options = args[0];
  var cb = args[1];

  var defaults = {
    rejectUnauthorized: '0' !== process.env.NODE_TLS_REJECT_UNAUTHORIZED,
    ciphers: tls.DEFAULT_CIPHERS,
    checkServerIdentity: tls.checkServerIdentity
  };

  options = util._extend(defaults, options || {});
  if (!options.keepAlive)
    options.singleUse = true;

  assert(typeof options.checkServerIdentity === 'function');

  var hostname = options.servername ||
                 options.host ||
                 (options.socket && options.socket._host) ||
                 'localhost';
  const NPN = {};
  const context = tls.createSecureContext(options);
  tls.convertNPNProtocols(options.NPNProtocols, NPN);

  var socket = new TLSSocket(options.socket, {
    pipe: options.path && !options.port,
    secureContext: context,
    isServer: false,
    requestCert: true,
    rejectUnauthorized: options.rejectUnauthorized,
    session: options.session,
    NPNProtocols: NPN.NPNProtocols,
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
        localAddress: options.localAddress
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
      socket.destroy(error);
    }
  }
  socket.once('end', onHangUp);

  return socket;
};
