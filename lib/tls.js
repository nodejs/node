var crypto = require('crypto');
var util = require('util');
var net = require('net');
var events = require('events');
var stream = require('stream');

var assert = process.assert;

var debugLevel = parseInt(process.env.NODE_DEBUG, 16);
var debug;
if (debugLevel & 0x2) {
  debug = function() { util.error.apply(this, arguments); };
} else {
  debug = function() { };
}


/* Lazy Loaded crypto object */
var SecureStream = null;

/**
 * Provides a pair of streams to do encrypted communication.
 */

function SecurePair(credentials, isServer, requestCert, rejectUnauthorized) {
  if (!(this instanceof SecurePair)) {
    return new SecurePair(credentials,
                          isServer,
                          requestCert,
                          rejectUnauthorized);
  }

  var self = this;

  try {
    SecureStream = process.binding('crypto').SecureStream;
  }
  catch (e) {
    throw new Error('node.js not compiled with openssl crypto support.');
  }

  events.EventEmitter.call(this);

  this._secureEstablished = false;
  this._isServer = isServer ? true : false;
  this._encWriteState = true;
  this._clearWriteState = true;
  this._done = false;

  var crypto = require('crypto');

  if (!credentials) {
    this.credentials = crypto.createCredentials();
  } else {
    this.credentials = credentials;
  }

  if (!this._isServer) {
    // For clients, we will always have either a given ca list or be using
    // default one
    requestCert = true;
  }

  this._secureEstablished = false;
  this._encInPending = [];
  this._clearInPending = [];

  this._rejectUnauthorized = rejectUnauthorized ? true : false;
  this._requestCert = requestCert ? true : false;

  this._ssl = new SecureStream(this.credentials.context,
                               this._isServer ? true : false,
                               this._requestCert,
                               this._rejectUnauthorized);


  /* Acts as a r/w stream to the cleartext side of the stream. */
  this.cleartext = new stream.Stream();
  this.cleartext.readable = true;
  this.cleartext.writable = true;

  /* Acts as a r/w stream to the encrypted side of the stream. */
  this.encrypted = new stream.Stream();
  this.encrypted.readable = true;
  this.encrypted.writable = true;

  this.cleartext.write = function(data) {
    if (typeof data == 'string') data = Buffer(data);
    debug('clearIn data');
    self._clearInPending.push(data);
    self._cycle();
    return self._cleartextWriteState;
  };

  this.cleartext.pause = function() {
    debug('paused cleartext');
    self._cleartextWriteState = false;
  };

  this.cleartext.resume = function() {
    debug('resumed cleartext');
    self._cleartextWriteState = true;
  };

  this.cleartext.end = function(err) {
    debug('cleartext end');
    if (!self._done) {
      self._ssl.shutdown();
      self._cycle();
    }
    self._destroy(err);
  };

  this.encrypted.write = function(data) {
    debug('encIn data');
    self._encInPending.push(data);
    self._cycle();
    return self._encryptedWriteState;
  };

  this.encrypted.pause = function() {
    if (typeof data == 'string') data = Buffer(data);
    debug('pause encrypted');
    self._encryptedWriteState = false;
  };

  this.encrypted.resume = function() {
    debug('resume encrypted');
    self._encryptedWriteState = true;
  };

  this.encrypted.end = function(err) {
    debug('encrypted end');
    if (!self._done) {
      self._ssl.shutdown();
      self._cycle();
    }
    self._destroy(err);
  };

  process.nextTick(function() {
    self._ssl.start();
    self._cycle();
  });
}

util.inherits(SecurePair, events.EventEmitter);


exports.createSecurePair = function(credentials,
                                    isServer,
                                    requestCert,
                                    rejectUnauthorized) {
  var pair = new SecurePair(credentials,
                            isServer,
                            requestCert,
                            rejectUnauthorized);
  return pair;
};


SecurePair.prototype._mover = function(reader, writer, checker) {
  var bytesRead;
  var pool;
  var chunkBytes;
  var chunk;

  do {
    bytesRead = 0;
    chunkBytes = 0;
    pool = new Buffer(4096); // alloc every time?
    pool.used = 0;

    do {
      try {
        chunkBytes = reader(pool,
                            pool.used + bytesRead,
                            pool.length - pool.used - bytesRead);
      } catch (e) {
        return this._error(e);
      }
      if (chunkBytes >= 0) {
        bytesRead += chunkBytes;
      }
    } while ((chunkBytes > 0) && (pool.used + bytesRead < pool.length));

    if (bytesRead > 0) {
      chunk = pool.slice(0, bytesRead);
      writer(chunk);
    }
  } while (bytesRead > 0 && checker());
};


/**
 * Attempt to cycle OpenSSLs buffers in various directions.
 *
 * An SSL Connection can be viewed as four separate piplines,
 * interacting with one has no connection to the behavoir of
 * any of the other 3 -- This might not sound reasonable,
 * but consider things like mid-stream renegotiation of
 * the ciphers.
 *
 * The four pipelines, using terminology of the client (server is just
 * reversed):
 *  (1) Encrypted Output stream (Writing encrypted data to peer)
 *  (2) Encrypted Input stream (Reading encrypted data from peer)
 *  (3) Cleartext Output stream (Decrypted content from the peer)
 *  (4) Cleartext Input stream (Cleartext content to send to the peer)
 *
 * This function attempts to pull any available data out of the Cleartext
 * input stream (4), and the Encrypted input stream (2).  Then it pushes any
 * data available from the cleartext output stream (3), and finally from the
 * Encrypted output stream (1)
 *
 * It is called whenever we do something with OpenSSL -- post reciving
 * content, trying to flush, trying to change ciphers, or shutting down the
 * connection.
 *
 * Because it is also called everywhere, we also check if the connection has
 * completed negotiation and emit 'secure' from here if it has.
 */
SecurePair.prototype._cycle = function() {
  if (this._done) {
    return;
  }

  var self = this;
  var rv;
  var tmp;
  var bytesRead;
  var bytesWritten;
  var chunk = null;
  var pool = null;

  // Pull in incoming encrypted data from the socket.
  // This arrives via some code like this:
  //
  //   socket.on('data', function (d) {
  //     pair.encrypted.write(d)
  //   });
  //
  var encPending = this._encInPending.length > 0;
  while (this._encInPending.length > 0) {
    tmp = this._encInPending.shift();

    try {
      debug('writing from encIn');
      rv = this._ssl.encIn(tmp, 0, tmp.length);
    } catch (e) {
      return this._error(e);
    }

    if (rv === 0) {
      this._encInPending.unshift(tmp);
      break;
    }

    assert(rv === tmp.length);
  }

  // If we've cleared all of incoming encrypted data, emit drain.
  if (encPending && this._encInPending.length === 0) {
    debug('encrypted drain');
    this.encrypted.emit('drain');
  }


  // Pull in any clear data coming from the application.
  // This arrives via some code like this:
  //
  //   pair.cleartext.write("hello world");
  //
  var clearPending = this._clearInPending.length > 0;
  while (this._clearInPending.length > 0) {
    tmp = this._clearInPending.shift();
    try {
      debug('writng from clearIn');
      rv = this._ssl.clearIn(tmp, 0, tmp.length);
    } catch (e) {
      return this._error(e);
    }

    if (rv === 0) {
      this._clearInPending.unshift(tmp);
      break;
    }

    assert(rv === tmp.length);
  }

  // If we've cleared all of incoming cleartext data, emit drain.
  if (clearPending && this._clearInPending.length === 0) {
    debug('cleartext drain');
    this.cleartext.emit('drain');
  }


  // Move decrypted, clear data out into the application.
  // From the user's perspective this occurs as a 'data' event
  // on the pair.cleartext.
  this._mover(
      function(pool, offset, length) {
        debug('reading from clearOut');
        return self._ssl.clearOut(pool, offset, length);
      },
      function(chunk) {
        self.cleartext.emit('data', chunk);
      },
      function() {
        return self._cleartextWriteState === true;
      });

  // Move encrypted data to the stream. From the user's perspective this
  // occurs as a 'data' event on the pair.encrypted. Usually the application
  // will have some code which pipes the stream to a socket:
  //
  //   pair.encrypted.on('data', function (d) {
  //     socket.write(d);
  //   });
  //
  this._mover(
      function(pool, offset, length) {
        debug('reading from encOut');
        if (!self._ssl) return -1;
        return self._ssl.encOut(pool, offset, length);
      },
      function(chunk) {
        self.encrypted.emit('data', chunk);
      },
      function() {
        if (!self._ssl) return false;
        return self._encryptedWriteState === true;
      });


  if (this._ssl && !this._secureEstablished && this._ssl.isInitFinished()) {
    this._secureEstablished = true;
    debug('secure established');
    this.emit('secure');
    this._cycle();
  }
};


SecurePair.prototype._destroy = function(err) {
  if (!this._done) {
    this._done = true;
    this._ssl.close();
    this._ssl = null;

    this.encrypted.emit('end');
    this.encrypted.emit('close');

    this.cleartext.emit('end');
    this.cleartext.emit('close');
  }
  this._cycle();
};


SecurePair.prototype._error = function(err) {
  if (this._isServer &&
      this._rejectUnauthorized &&
      /peer did not return a certificate/.test(err.message)) {
    // Not really an error.
    this._destroy();
  } else {
    this.emit('error', err);
  }
};


SecurePair.prototype.getPeerCertificate = function(err) {
  if (this._ssl) {
    return this._ssl.getPeerCertificate();
  } else {
    return null;
  }
};


SecurePair.prototype.getCipher = function(err) {
  if (this._ssl) {
    return this._ssl.getCurrentCipher();
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
//   false.
//
//
//
// Options:
// - requestCert. Send verify request. Default to false.
// - rejectUnauthorized. Boolean, default to false.
// - key. string.
// - cert: string.
// - ca: string or array of strings.
//
// emit 'secureConnection'
//   function (cleartextStream, encryptedStream) { }
//
//   'cleartextStream' has the boolean property 'authorized' to determine if
//   it was verified by the CA. If 'authorized' is false, a property
//   'authorizationError' is set on cleartextStream and has the possible
//   values:
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
//
// TODO:
// cleartext.credentials (by mirroring from pair object)
// cleartext.getCertificate() (by mirroring from pair.credentials.context)
function Server(/* [options], listener */) {
  var options, listener;
  if (typeof arguments[0] == 'object') {
    options = arguments[0];
    listener = arguments[1];
  } else if (typeof arguments[0] == 'function') {
    options = {};
    listener = arguments[0];
  }

  if (!(this instanceof Server)) return new Server(options, listener);

  var self = this;

  // constructor call
  net.Server.call(this, function(socket) {
    var creds = crypto.createCredentials(
        { key: self.key, cert: self.cert, ca: self.ca });
    creds.context.setCiphers('RC4-SHA:AES128-SHA:AES256-SHA');

    var pair = new SecurePair(creds,
                              true,
                              self.requestCert,
                              self.rejectUnauthorized);
    pair.encrypted.pipe(socket);
    socket.pipe(pair.encrypted);

    pair.on('secure', function() {
      pair.cleartext.authorized = false;
      if (!self.requestCert) {
        self.emit('secureConnection', pair.cleartext, pair.encrypted);
      } else {
        var verifyError = pair._ssl.verifyError();
        if (verifyError) {
          pair.cleartext.authorizationError = verifyError;

          if (self.rejectUnauthorized) {
            socket.destroy();
            pair._destroy();
          } else {
            self.emit('secureConnection', pair.cleartext, pair.encrypted);
          }
        } else {
          pair.cleartext.authorized = true;
          self.emit('secureConnection', pair.cleartext, pair.encrypted);
        }
      }
    });

    pair.on('error', function(e) {
      console.error('pair got error: ' + e);
      self.emit('error', e);
    });

    pair.cleartext.on('error', function(err) {
      console.error('cleartext got error: ' + err);
    });

    pair.encrypted.on('error', function(err) {
      console.log('encrypted got error: ' + err);
    });
  });

  if (listener) {
    this.on('secureConnection', listener);
  }

  // Handle option defaults:
  this.setOptions(options);
}

util.inherits(Server, net.Server);
exports.Server = Server;
exports.createServer = function(options, listener) {
  return new Server(options, listener);
};


Server.prototype.setOptions = function(options) {
  if (typeof options.requestCert == 'boolean') {
    this.requestCert = options.requestCert;
  } else {
    this.requestCert = false;
  }

  if (typeof options.rejectUnauthorized == 'boolean') {
    this.rejectUnauthorized = options.rejectUnauthorized;
  } else {
    this.rejectUnauthorized = false;
  }

  if (options.key) this.key = options.key;
  if (options.cert) this.cert = options.cert;
  if (options.ca) this.ca = options.ca;
};

