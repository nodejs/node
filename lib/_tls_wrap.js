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

var assert = require('assert');
var constants = require('constants');
var crypto = require('crypto');
var net = require('net');
var tls = require('tls');
var util = require('util');

var Timer = process.binding('timer_wrap').Timer;
var tls_wrap = process.binding('tls_wrap');

// Lazy load
var tls_legacy;

var debug = util.debuglog('tls');

function onhandshakestart() {
  debug('onhandshakestart');

  var self = this;
  var ssl = self.ssl;
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
      self._tlsError(err);
    });
  }
}


function onhandshakedone() {
  // for future use
  debug('onhandshakedone');
  this._finishInit();
}


function onclienthello(hello) {
  var self = this,
      onceSession = false,
      onceSNI = false;

  function callback(err, session) {
    if (onceSession)
      return self.destroy(new Error('TLS session callback was called 2 times'));
    onceSession = true;

    if (err)
      return self.destroy(err);

    // NOTE: That we have disabled OpenSSL's internal session storage in
    // `node_crypto.cc` and hence its safe to rely on getting servername only
    // from clienthello or this place.
    var ret = self.ssl.loadSession(session);

    // Servername came from SSL session
    // NOTE: TLS Session ticket doesn't include servername information
    //
    // Another note, From RFC3546:
    //
    //   If, on the other hand, the older
    //   session is resumed, then the server MUST ignore extensions appearing
    //   in the client hello, and send a server hello containing no
    //   extensions; in this case the extension functionality negotiated
    //   during the original session initiation is applied to the resumed
    //   session.
    //
    // Therefore we should account session loading when dealing with servername
    if (ret && ret.servername) {
      self._SNICallback(ret.servername, onSNIResult);
    } else if (hello.servername && self._SNICallback) {
      self._SNICallback(hello.servername, onSNIResult);
    } else {
      self.ssl.endParser();
    }
  }

  function onSNIResult(err, context) {
    if (onceSNI)
      return self.destroy(new Error('TLS SNI callback was called 2 times'));
    onceSNI = true;

    if (err)
      return self.destroy(err);

    if (context)
      self.ssl.sni_context = context;

    self.ssl.endParser();
  }

  if (hello.sessionId.length <= 0 ||
      hello.tlsTicket ||
      this.server &&
      !this.server.emit('resumeSession', hello.sessionId, callback)) {
    // Invoke SNI callback, since we've no session to resume
    if (hello.servername && this._SNICallback)
      this._SNICallback(hello.servername, onSNIResult);
    else
      this.ssl.endParser();
  }
}


function onnewsession(key, session) {
  if (this.server)
    this.server.emit('newSession', key, session);
}


/**
 * Provides a wrap of socket stream to do encrypted communication.
 */

function TLSSocket(socket, options) {
  // Disallow wrapping TLSSocket in TLSSocket
  assert(!(socket instanceof TLSSocket));

  net.Socket.call(this, socket && {
    handle: socket._handle,
    allowHalfOpen: socket.allowHalfOpen,
    readable: socket.readable,
    writable: socket.writable
  });

  this._tlsOptions = options;
  this._secureEstablished = false;
  this._controlReleased = false;
  this._SNICallback = null;
  this.ssl = null;
  this.servername = null;
  this.npnProtocol = null;
  this.authorized = false;
  this.authorizationError = null;

  this.on('error', this._tlsError);

  if (!this._handle)
    this.once('connect', this._init.bind(this));
  else
    this._init();
}
util.inherits(TLSSocket, net.Socket);
exports.TLSSocket = TLSSocket;

TLSSocket.prototype._init = function() {
  assert(this._handle);

  // lib/net.js expect this value to be non-zero if write hasn't been flushed
  // immediately
  // TODO(indutny): rewise this solution, it might be 1 before handshake and
  // represent real writeQueueSize during regular writes.
  this._handle.writeQueueSize = 1;

  var self = this;
  var options = this._tlsOptions;

  // Wrap socket's handle
  var credentials = options.credentials || crypto.createCredentials();
  this.ssl = tls_wrap.wrap(this._handle, credentials.context, options.isServer);
  this.server = options.server || null;

  // For clients, we will always have either a given ca list or be using
  // default one
  var requestCert = !!options.requestCert || !options.isServer,
      rejectUnauthorized = !!options.rejectUnauthorized;

  this._requestCert = requestCert;
  this._rejectUnauthorized = rejectUnauthorized;
  if (requestCert || rejectUnauthorized)
    this.ssl.setVerifyMode(requestCert, rejectUnauthorized);

  if (options.isServer) {
    this.ssl.onhandshakestart = onhandshakestart.bind(this);
    this.ssl.onhandshakedone = onhandshakedone.bind(this);
    this.ssl.onclienthello = onclienthello.bind(this);
    this.ssl.onnewsession = onnewsession.bind(this);
    this.ssl.lastHandshakeTime = 0;
    this.ssl.handshakes = 0;

    if (this.server &&
        (this.server.listeners('resumeSession').length > 0 ||
         this.server.listeners('newSession').length > 0)) {
      this.ssl.enableSessionCallbacks();
    }
  } else {
    this.ssl.onhandshakestart = function() {};
    this.ssl.onhandshakedone = this._finishInit.bind(this);
  }

  this.ssl.onerror = function(err) {
    // Destroy socket if error happened before handshake's finish
    if (!this._secureEstablished) {
      self._tlsError(err);
      self.destroy();
    } else if (options.isServer &&
               rejectUnauthorized &&
               /peer did not return a certificate/.test(err.message)) {
      // Ignore server's authorization errors
      self.destroy();
    } else {
      // Throw error
      self._tlsError(err);
    }
  };

  // If custom SNICallback was given, or if
  // there're SNI contexts to perform match against -
  // set `.onsniselect` callback.
  if (process.features.tls_sni &&
      options.isServer &&
      options.server &&
      (options.SNICallback !== SNICallback ||
       options.server._contexts.length)) {
    assert(typeof options.SNICallback === 'function');
    this._SNICallback = options.SNICallback;
    this.ssl.enableHelloParser();
  }

  if (process.features.tls_npn && options.NPNProtocols)
    this.ssl.setNPNProtocols(options.NPNProtocols);

  if (options.handshakeTimeout > 0)
    this.setTimeout(options.handshakeTimeout, this._handleTimeout);
};

TLSSocket.prototype.renegotiate = function(options, callback) {
  var requestCert = this._requestCert,
      rejectUnauthorized = this._rejectUnauthorized;

  if (typeof options.requestCert !== 'undefined')
    requestCert = !!options.requestCert;
  if (typeof options.rejectUnauthorized !== 'undefined')
    rejectUnauthorized = !!options.rejectUnauthorized;

  if (requestCert !== this._requestCert ||
      rejectUnauthorized !== this._rejectUnauthorized) {
    this.ssl.setVerifyMode(requestCert, rejectUnauthorized);
    this._requestCert = requestCert;
    this._rejectUnauthorized = rejectUnauthorized;
  }
  if (!this.ssl.renegotiate()) {
    if (callback) {
      process.nextTick(function() {
        callback(new Error('Failed to renegotiate'));
      });
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

TLSSocket.prototype._handleTimeout = function() {
  this._tlsError(new Error('TLS handshake timeout'));
};

TLSSocket.prototype._tlsError = function(err) {
  this.emit('_tlsError', err);
  if (this._controlReleased)
    this.emit('error', err);
};

TLSSocket.prototype._releaseControl = function() {
  if (this._controlReleased)
    return false;
  this._controlReleased = true;
  this.removeListener('error', this._tlsError);
  return true;
};

TLSSocket.prototype._finishInit = function() {
  if (process.features.tls_npn) {
    this.npnProtocol = this.ssl.getNegotiatedProtocol();
  }

  if (process.features.tls_sni && this._tlsOptions.isServer) {
    this.servername = this.ssl.getServername();
  }

  debug('secure established');
  this._secureEstablished = true;
  if (this._tlsOptions.handshakeTimeout > 0)
    this.setTimeout(0, this._handleTimeout);
  this.emit('secure');
};

TLSSocket.prototype._start = function() {
  this.ssl.start();
};

TLSSocket.prototype.setServername = function(name) {
  this.ssl.setServername(name);
};

TLSSocket.prototype.setSession = function(session) {
  if (util.isString(session))
    session = new Buffer(session, 'binary');
  this.ssl.setSession(session);
};

TLSSocket.prototype.getPeerCertificate = function() {
  if (this.ssl) {
    var c = this.ssl.getPeerCertificate();

    if (c) {
      if (c.issuer) c.issuer = tls.parseCertString(c.issuer);
      if (c.subject) c.subject = tls.parseCertString(c.subject);
      return c;
    }
  }

  return null;
};

TLSSocket.prototype.getSession = function() {
  if (this.ssl) {
    return this.ssl.getSession();
  }

  return null;
};

TLSSocket.prototype.isSessionReused = function() {
  if (this.ssl) {
    return this.ssl.isSessionReused();
  }

  return null;
};

TLSSocket.prototype.getCipher = function(err) {
  if (this.ssl) {
    return this.ssl.getCurrentCipher();
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
  if (util.isObject(arguments[0])) {
    options = arguments[0];
    listener = arguments[1];
  } else if (util.isFunction(arguments[0])) {
    options = {};
    listener = arguments[0];
  }

  if (!(this instanceof Server)) return new Server(options, listener);

  this._contexts = [];

  var self = this;

  // Handle option defaults:
  this.setOptions(options);

  if (!self.pfx && (!self.cert || !self.key)) {
    throw new Error('Missing PFX or certificate + private key.');
  }

  var sharedCreds = crypto.createCredentials({
    pfx: self.pfx,
    key: self.key,
    passphrase: self.passphrase,
    cert: self.cert,
    ca: self.ca,
    ciphers: self.ciphers || tls.DEFAULT_CIPHERS,
    secureProtocol: self.secureProtocol,
    secureOptions: self.secureOptions,
    crl: self.crl,
    sessionIdContext: self.sessionIdContext
  });
  this._sharedCreds = sharedCreds;

  var timeout = options.handshakeTimeout || (120 * 1000);

  if (!util.isNumber(timeout)) {
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
      credentials: sharedCreds,
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
        var verifyError = socket.ssl.verifyError();
        if (verifyError) {
          socket.authorizationError = verifyError.message;

          if (socket._rejectUnauthorized)
            socket.destroy();
        } else {
          socket.authorized = true;
        }
      }

      if (!socket.destroyed && socket._releaseControl())
        self.emit('secureConnection', socket);
    });

    socket.on('_tlsError', function(err) {
      if (!socket._controlReleased)
        self.emit('clientError', err, socket);
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
    ticketKeys: this._sharedCreds.context.getTicketKeys().toString('hex')
  };
};


Server.prototype._setServerData = function(data) {
  this._sharedCreds.context.setTicketKeys(new Buffer(data.ticketKeys, 'hex'));
};


Server.prototype.setOptions = function(options) {
  if (util.isBoolean(options.requestCert)) {
    this.requestCert = options.requestCert;
  } else {
    this.requestCert = false;
  }

  if (util.isBoolean(options.rejectUnauthorized)) {
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
  if (options.sessionTimeout) this.sessionTimeout = options.sessionTimeout;
  var secureOptions = options.secureOptions || 0;
  if (options.honorCipherOrder) {
    secureOptions |= constants.SSL_OP_CIPHER_SERVER_PREFERENCE;
  }
  if (secureOptions) this.secureOptions = secureOptions;
  if (options.NPNProtocols) tls.convertNPNProtocols(options.NPNProtocols, this);
  if (options.sessionIdContext) {
    this.sessionIdContext = options.sessionIdContext;
  } else {
    this.sessionIdContext = crypto.createHash('md5')
                                  .update(process.argv.join(' '))
                                  .digest('hex');
  }
};

// SNI Contexts High-Level API
Server.prototype.addContext = function(servername, credentials) {
  if (!servername) {
    throw 'Servername is required parameter for Server.addContext';
  }

  var re = new RegExp('^' +
                      servername.replace(/([\.^$+?\-\\[\]{}])/g, '\\$1')
                                .replace(/\*/g, '.*') +
                      '$');
  this._contexts.push([re, crypto.createCredentials(credentials).context]);
};

function SNICallback(servername, callback) {
  var ctx;

  this.server._contexts.some(function(elem) {
    if (!util.isNull(servername.match(elem[0]))) {
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

  if (util.isObject(listArgs[1])) {
    options = util._extend(options, listArgs[1]);
  } else if (util.isObject(listArgs[2])) {
    options = util._extend(options, listArgs[2]);
  }

  return (cb) ? [options, cb] : [options];
}

function legacyConnect(hostname, options, NPN, credentials) {
  assert(options.socket);
  if (!tls_legacy)
    tls_legacy = require('_tls_legacy');

  var pair = tls_legacy.createSecurePair(credentials,
                                         false,
                                         true,
                                         !!options.rejectUnauthorized,
                                         {
                                           NPNProtocols: NPN.NPNProtocols,
                                           servername: hostname
                                         });
  tls_legacy.pipe(pair, options.socket);
  pair.cleartext._controlReleased = true;
  pair.on('error', function(err) {
    pair.cleartext.emit('error', err);
  });

  return pair;
}

exports.connect = function(/* [port, host], options, cb */) {
  var args = normalizeConnectArgs(arguments);
  var options = args[0];
  var cb = args[1];

  var defaults = {
    rejectUnauthorized: '0' !== process.env.NODE_TLS_REJECT_UNAUTHORIZED
  };
  options = util._extend(defaults, options || {});

  var hostname = options.servername ||
                 options.host ||
                 options.socket && options.socket._host,
      NPN = {},
      credentials = crypto.createCredentials(options);
  tls.convertNPNProtocols(options.NPNProtocols, NPN);

  // Wrapping TLS socket inside another TLS socket was requested -
  // create legacy secure pair
  var socket;
  var legacy;
  var result;
  if (options.socket instanceof TLSSocket) {
    debug('legacy connect');
    legacy = true;
    socket = legacyConnect(hostname, options, NPN, credentials);
    result = socket.cleartext;
  } else {
    legacy = false;
    socket = new TLSSocket(options.socket, {
      credentials: credentials,
      isServer: false,
      requestCert: true,
      rejectUnauthorized: options.rejectUnauthorized,
      NPNProtocols: NPN.NPNProtocols
    });
    result = socket;
  }

  if (socket._handle)
    onHandle();
  else
    socket.once('connect', onHandle);

  if (cb)
    result.once('secureConnect', cb);

  if (!options.socket) {
    assert(!legacy);
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
    socket.connect(connect_opt);
  }

  return result;

  function onHandle() {
    if (!legacy)
      socket._releaseControl();

    if (options.session)
      socket.setSession(options.session);

    if (!legacy) {
      if (options.servername)
        socket.setServername(options.servername);

      socket._start();
    }
    socket.on('secure', function() {
      var verifyError = socket.ssl.verifyError();

      // Verify that server's identity matches it's certificate's names
      if (!verifyError) {
        var cert = result.getPeerCertificate();
        var validCert = tls.checkServerIdentity(hostname, cert);
        if (!validCert) {
          verifyError = new Error('Hostname/IP doesn\'t match certificate\'s ' +
                                  'altnames');
        }
      }

      if (verifyError) {
        result.authorized = false;
        result.authorizationError = verifyError.message;

        if (options.rejectUnauthorized) {
          result.emit('error', verifyError);
          result.destroy();
          return;
        } else {
          result.emit('secureConnect');
        }
      } else {
        result.authorized = true;
        result.emit('secureConnect');
      }

      // Uncork incoming data
      result.removeListener('end', onHangUp);
    });

    function onHangUp() {
      // NOTE: This logic is shared with _http_client.js
      if (!socket._hadError) {
        socket._hadError = true;
        var error = new Error('socket hang up');
        error.code = 'ECONNRESET';
        socket.destroy();
        socket.emit('error', error);
      }
    }
    result.once('end', onHangUp);
  }
};
