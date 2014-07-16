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
var events = require('events');
var stream = require('stream');
var tls = require('tls');
var util = require('util');
var common = require('_tls_common');

var Timer = process.binding('timer_wrap').Timer;
var Connection = null;
try {
  Connection = process.binding('crypto').Connection;
} catch (e) {
  throw new Error('node.js not compiled with openssl crypto support.');
}

var debug = util.debuglog('tls-legacy');

function SlabBuffer() {
  this.create();
}


SlabBuffer.prototype.create = function create() {
  this.isFull = false;
  this.pool = new Buffer(tls.SLAB_BUFFER_SIZE);
  this.offset = 0;
  this.remaining = this.pool.length;
};


SlabBuffer.prototype.use = function use(context, fn, size) {
  if (this.remaining === 0) {
    this.isFull = true;
    return 0;
  }

  var actualSize = this.remaining;

  if (!util.isNull(size)) actualSize = Math.min(size, actualSize);

  var bytes = fn.call(context, this.pool, this.offset, actualSize);
  if (bytes > 0) {
    this.offset += bytes;
    this.remaining -= bytes;
  }

  assert(this.remaining >= 0);

  return bytes;
};


var slabBuffer = null;


// Base class of both CleartextStream and EncryptedStream
function CryptoStream(pair, options) {
  stream.Duplex.call(this, options);

  this.pair = pair;
  this._pending = null;
  this._pendingEncoding = '';
  this._pendingCallback = null;
  this._doneFlag = false;
  this._retryAfterPartial = false;
  this._halfRead = false;
  this._sslOutCb = null;
  this._resumingSession = false;
  this._reading = true;
  this._destroyed = false;
  this._ended = false;
  this._finished = false;
  this._opposite = null;

  if (util.isNull(slabBuffer)) slabBuffer = new SlabBuffer();
  this._buffer = slabBuffer;

  this.once('finish', onCryptoStreamFinish);

  // net.Socket calls .onend too
  this.once('end', onCryptoStreamEnd);
}
util.inherits(CryptoStream, stream.Duplex);


function onCryptoStreamFinish() {
  this._finished = true;

  if (this === this.pair.cleartext) {
    debug('cleartext.onfinish');
    if (this.pair.ssl) {
      // Generate close notify
      // NOTE: first call checks if client has sent us shutdown,
      // second call enqueues shutdown into the BIO.
      if (this.pair.ssl.shutdown() !== 1) {
        if (this.pair.ssl && this.pair.ssl.error)
          return this.pair.error();

        this.pair.ssl.shutdown();
      }

      if (this.pair.ssl && this.pair.ssl.error)
        return this.pair.error();
    }
  } else {
    debug('encrypted.onfinish');
  }

  // Try to read just to get sure that we won't miss EOF
  if (this._opposite.readable) this._opposite.read(0);

  if (this._opposite._ended) {
    this._done();

    // No half-close, sorry
    if (this === this.pair.cleartext) this._opposite._done();
  }
}


function onCryptoStreamEnd() {
  this._ended = true;
  if (this === this.pair.cleartext) {
    debug('cleartext.onend');
  } else {
    debug('encrypted.onend');
  }
}


// NOTE: Called once `this._opposite` is set.
CryptoStream.prototype.init = function init() {
  var self = this;
  this._opposite.on('sslOutEnd', function() {
    if (self._sslOutCb) {
      var cb = self._sslOutCb;
      self._sslOutCb = null;
      cb(null);
    }
  });
};


CryptoStream.prototype._write = function write(data, encoding, cb) {
  assert(util.isNull(this._pending));

  // Black-hole data
  if (!this.pair.ssl) return cb(null);

  // When resuming session don't accept any new data.
  // And do not put too much data into openssl, before writing it from encrypted
  // side.
  //
  // TODO(indutny): Remove magic number, use watermark based limits
  if (!this._resumingSession &&
      this._opposite._internallyPendingBytes() < 128 * 1024) {
    // Write current buffer now
    var written;
    if (this === this.pair.cleartext) {
      debug('cleartext.write called with %d bytes', data.length);
      written = this.pair.ssl.clearIn(data, 0, data.length);
    } else {
      debug('encrypted.write called with %d bytes', data.length);
      written = this.pair.ssl.encIn(data, 0, data.length);
    }

    // Handle and report errors
    if (this.pair.ssl && this.pair.ssl.error) {
      return cb(this.pair.error(true));
    }

    // Force SSL_read call to cycle some states/data inside OpenSSL
    this.pair.cleartext.read(0);

    // Cycle encrypted data
    if (this.pair.encrypted._internallyPendingBytes())
      this.pair.encrypted.read(0);

    // Get NPN and Server name when ready
    this.pair.maybeInitFinished();

    // Whole buffer was written
    if (written === data.length) {
      if (this === this.pair.cleartext) {
        debug('cleartext.write succeed with ' + written + ' bytes');
      } else {
        debug('encrypted.write succeed with ' + written + ' bytes');
      }

      // Invoke callback only when all data read from opposite stream
      if (this._opposite._halfRead) {
        assert(util.isNull(this._sslOutCb));
        this._sslOutCb = cb;
      } else {
        cb(null);
      }
      return;
    } else if (written !== 0 && written !== -1) {
      assert(!this._retryAfterPartial);
      this._retryAfterPartial = true;
      this._write(data.slice(written), encoding, cb);
      this._retryAfterPartial = false;
      return;
    }
  } else {
    debug('cleartext.write queue is full');

    // Force SSL_read call to cycle some states/data inside OpenSSL
    this.pair.cleartext.read(0);
  }

  // No write has happened
  this._pending = data;
  this._pendingEncoding = encoding;
  this._pendingCallback = cb;

  if (this === this.pair.cleartext) {
    debug('cleartext.write queued with %d bytes', data.length);
  } else {
    debug('encrypted.write queued with %d bytes', data.length);
  }
};


CryptoStream.prototype._writePending = function writePending() {
  var data = this._pending,
      encoding = this._pendingEncoding,
      cb = this._pendingCallback;

  this._pending = null;
  this._pendingEncoding = '';
  this._pendingCallback = null;
  this._write(data, encoding, cb);
};


CryptoStream.prototype._read = function read(size) {
  // XXX: EOF?!
  if (!this.pair.ssl) return this.push(null);

  // Wait for session to be resumed
  // Mark that we're done reading, but don't provide data or EOF
  if (this._resumingSession || !this._reading) return this.push('');

  var out;
  if (this === this.pair.cleartext) {
    debug('cleartext.read called with %d bytes', size);
    out = this.pair.ssl.clearOut;
  } else {
    debug('encrypted.read called with %d bytes', size);
    out = this.pair.ssl.encOut;
  }

  var bytesRead = 0,
      start = this._buffer.offset,
      last = start;
  do {
    assert(last === this._buffer.offset);
    var read = this._buffer.use(this.pair.ssl, out, size - bytesRead);
    if (read > 0) {
      bytesRead += read;
    }
    last = this._buffer.offset;

    // Handle and report errors
    if (this.pair.ssl && this.pair.ssl.error) {
      this.pair.error();
      break;
    }
  } while (read > 0 &&
           !this._buffer.isFull &&
           bytesRead < size &&
           this.pair.ssl !== null);

  // Get NPN and Server name when ready
  this.pair.maybeInitFinished();

  // Create new buffer if previous was filled up
  var pool = this._buffer.pool;
  if (this._buffer.isFull) this._buffer.create();

  assert(bytesRead >= 0);

  if (this === this.pair.cleartext) {
    debug('cleartext.read succeed with %d bytes', bytesRead);
  } else {
    debug('encrypted.read succeed with %d bytes', bytesRead);
  }

  // Try writing pending data
  if (!util.isNull(this._pending)) this._writePending();
  if (!util.isNull(this._opposite._pending)) this._opposite._writePending();

  if (bytesRead === 0) {
    // EOF when cleartext has finished and we have nothing to read
    if (this._opposite._finished && this._internallyPendingBytes() === 0 ||
        this.pair.ssl && this.pair.ssl.receivedShutdown) {
      // Perform graceful shutdown
      this._done();

      // No half-open, sorry!
      if (this === this.pair.cleartext) {
        this._opposite._done();

        // EOF
        this.push(null);
      } else if (!this.pair.ssl || !this.pair.ssl.receivedShutdown) {
        // EOF
        this.push(null);
      }
    } else {
      // Bail out
      this.push('');
    }
  } else {
    // Give them requested data
    this.push(pool.slice(start, start + bytesRead));
  }

  // Let users know that we've some internal data to read
  var halfRead = this._internallyPendingBytes() !== 0;

  // Smart check to avoid invoking 'sslOutEnd' in the most of the cases
  if (this._halfRead !== halfRead) {
    this._halfRead = halfRead;

    // Notify listeners about internal data end
    if (!halfRead) {
      if (this === this.pair.cleartext) {
        debug('cleartext.sslOutEnd');
      } else {
        debug('encrypted.sslOutEnd');
      }

      this.emit('sslOutEnd');
    }
  }
};


CryptoStream.prototype.setTimeout = function(timeout, callback) {
  if (this.socket) this.socket.setTimeout(timeout, callback);
};


CryptoStream.prototype.setNoDelay = function(noDelay) {
  if (this.socket) this.socket.setNoDelay(noDelay);
};


CryptoStream.prototype.setKeepAlive = function(enable, initialDelay) {
  if (this.socket) this.socket.setKeepAlive(enable, initialDelay);
};

CryptoStream.prototype.__defineGetter__('bytesWritten', function() {
  return this.socket ? this.socket.bytesWritten : 0;
});

CryptoStream.prototype.getPeerCertificate = function(detailed) {
  if (this.pair.ssl) {
    return common.translatePeerCertificate(
        this.pair.ssl.getPeerCertificate(detailed));
  }

  return null;
};

CryptoStream.prototype.getSession = function() {
  if (this.pair.ssl) {
    return this.pair.ssl.getSession();
  }

  return null;
};

CryptoStream.prototype.isSessionReused = function() {
  if (this.pair.ssl) {
    return this.pair.ssl.isSessionReused();
  }

  return null;
};

CryptoStream.prototype.getCipher = function(err) {
  if (this.pair.ssl) {
    return this.pair.ssl.getCurrentCipher();
  } else {
    return null;
  }
};


CryptoStream.prototype.end = function(chunk, encoding) {
  if (this === this.pair.cleartext) {
    debug('cleartext.end');
  } else {
    debug('encrypted.end');
  }

  // Write pending data first
  if (!util.isNull(this._pending)) this._writePending();

  this.writable = false;

  stream.Duplex.prototype.end.call(this, chunk, encoding);
};


CryptoStream.prototype.destroySoon = function(err) {
  if (this === this.pair.cleartext) {
    debug('cleartext.destroySoon');
  } else {
    debug('encrypted.destroySoon');
  }

  if (this.writable)
    this.end();

  if (this._writableState.finished && this._opposite._ended) {
    this.destroy();
  } else {
    // Wait for both `finish` and `end` events to ensure that all data that
    // was written on this side was read from the other side.
    var self = this;
    var waiting = 1;
    function finish() {
      if (--waiting === 0) self.destroy();
    }
    this._opposite.once('end', finish);
    if (!this._finished) {
      this.once('finish', finish);
      ++waiting;
    }
  }
};


CryptoStream.prototype.destroy = function(err) {
  if (this._destroyed) return;
  this._destroyed = true;
  this.readable = this.writable = false;

  // Destroy both ends
  if (this === this.pair.cleartext) {
    debug('cleartext.destroy');
  } else {
    debug('encrypted.destroy');
  }
  this._opposite.destroy();

  var self = this;
  process.nextTick(function() {
    // Force EOF
    self.push(null);

    // Emit 'close' event
    self.emit('close', err ? true : false);
  });
};


CryptoStream.prototype._done = function() {
  this._doneFlag = true;

  if (this === this.pair.encrypted && !this.pair._secureEstablished)
    return this.pair.error();

  if (this.pair.cleartext._doneFlag &&
      this.pair.encrypted._doneFlag &&
      !this.pair._doneFlag) {
    // If both streams are done:
    this.pair.destroy();
  }
};


// readyState is deprecated. Don't use it.
Object.defineProperty(CryptoStream.prototype, 'readyState', {
  get: function() {
    if (this._connecting) {
      return 'opening';
    } else if (this.readable && this.writable) {
      return 'open';
    } else if (this.readable && !this.writable) {
      return 'readOnly';
    } else if (!this.readable && this.writable) {
      return 'writeOnly';
    } else {
      return 'closed';
    }
  }
});


function CleartextStream(pair, options) {
  CryptoStream.call(this, pair, options);

  // This is a fake kludge to support how the http impl sits
  // on top of net Sockets
  var self = this;
  this._handle = {
    readStop: function() {
      self._reading = false;
    },
    readStart: function() {
      if (self._reading && self._readableState.length > 0) return;
      self._reading = true;
      self.read(0);
      if (self._opposite.readable) self._opposite.read(0);
    }
  };
}
util.inherits(CleartextStream, CryptoStream);


CleartextStream.prototype._internallyPendingBytes = function() {
  if (this.pair.ssl) {
    return this.pair.ssl.clearPending();
  } else {
    return 0;
  }
};


CleartextStream.prototype.address = function() {
  return this.socket && this.socket.address();
};


CleartextStream.prototype.__defineGetter__('remoteAddress', function() {
  return this.socket && this.socket.remoteAddress;
});

CleartextStream.prototype.__defineGetter__('remoteFamily', function() {
  return this.socket && this.socket.remoteFamily;
});

CleartextStream.prototype.__defineGetter__('remotePort', function() {
  return this.socket && this.socket.remotePort;
});


CleartextStream.prototype.__defineGetter__('localAddress', function() {
  return this.socket && this.socket.localAddress;
});


CleartextStream.prototype.__defineGetter__('localPort', function() {
  return this.socket && this.socket.localPort;
});


function EncryptedStream(pair, options) {
  CryptoStream.call(this, pair, options);
}
util.inherits(EncryptedStream, CryptoStream);


EncryptedStream.prototype._internallyPendingBytes = function() {
  if (this.pair.ssl) {
    return this.pair.ssl.encPending();
  } else {
    return 0;
  }
};


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
      if (self.cleartext) self.cleartext.emit('error', err);
    });
  }
}


function onhandshakedone() {
  // for future use
  debug('onhandshakedone');
}


function onclienthello(hello) {
  var self = this,
      once = false;

  this._resumingSession = true;
  function callback(err, session) {
    if (once) return;
    once = true;

    if (err) return self.socket.destroy(err);

    self.ssl.loadSession(session);
    self.ssl.endParser();

    // Cycle data
    self._resumingSession = false;
    self.cleartext.read(0);
    self.encrypted.read(0);
  }

  if (hello.sessionId.length <= 0 ||
      !this.server ||
      !this.server.emit('resumeSession', hello.sessionId, callback)) {
    callback(null, null);
  }
}


function onnewsession(key, session) {
  if (!this.server) return;

  var self = this;
  var once = false;

  self.server.emit('newSession', key, session, function() {
    if (once)
      return;
    once = true;

    if (self.ssl)
      self.ssl.newSessionDone();
  });
}


function onocspresponse(resp) {
  this.emit('OCSPResponse', resp);
}


/**
 * Provides a pair of streams to do encrypted communication.
 */

function SecurePair(context, isServer, requestCert, rejectUnauthorized,
                    options) {
  if (!(this instanceof SecurePair)) {
    return new SecurePair(context,
                          isServer,
                          requestCert,
                          rejectUnauthorized,
                          options);
  }

  var self = this;

  options || (options = {});

  events.EventEmitter.call(this);

  this.server = options.server;
  this._secureEstablished = false;
  this._isServer = isServer ? true : false;
  this._encWriteState = true;
  this._clearWriteState = true;
  this._doneFlag = false;
  this._destroying = false;

  if (!context) {
    this.credentials = tls.createSecureContext();
  } else {
    this.credentials = context;
  }

  if (!this._isServer) {
    // For clients, we will always have either a given ca list or be using
    // default one
    requestCert = true;
  }

  this._rejectUnauthorized = rejectUnauthorized ? true : false;
  this._requestCert = requestCert ? true : false;

  this.ssl = new Connection(this.credentials.context,
                            this._isServer ? true : false,
                            this._isServer ? this._requestCert :
                                             options.servername,
                            this._rejectUnauthorized);

  if (this._isServer) {
    this.ssl.onhandshakestart = onhandshakestart.bind(this);
    this.ssl.onhandshakedone = onhandshakedone.bind(this);
    this.ssl.onclienthello = onclienthello.bind(this);
    this.ssl.onnewsession = onnewsession.bind(this);
    this.ssl.lastHandshakeTime = 0;
    this.ssl.handshakes = 0;
  } else {
    this.ssl.onocspresponse = onocspresponse.bind(this);
  }

  if (process.features.tls_sni) {
    if (this._isServer && options.SNICallback) {
      this.ssl.setSNICallback(options.SNICallback);
    }
    this.servername = null;
  }

  if (process.features.tls_npn && options.NPNProtocols) {
    this.ssl.setNPNProtocols(options.NPNProtocols);
    this.npnProtocol = null;
  }

  /* Acts as a r/w stream to the cleartext side of the stream. */
  this.cleartext = new CleartextStream(this, options.cleartext);

  /* Acts as a r/w stream to the encrypted side of the stream. */
  this.encrypted = new EncryptedStream(this, options.encrypted);

  /* Let streams know about each other */
  this.cleartext._opposite = this.encrypted;
  this.encrypted._opposite = this.cleartext;
  this.cleartext.init();
  this.encrypted.init();

  process.nextTick(function() {
    /* The Connection may be destroyed by an abort call */
    if (self.ssl) {
      self.ssl.start();

      if (options.requestOCSP)
        self.ssl.requestOCSP();

      /* In case of cipher suite failures - SSL_accept/SSL_connect may fail */
      if (self.ssl && self.ssl.error)
        self.error();
    }
  });
}

util.inherits(SecurePair, events.EventEmitter);


exports.createSecurePair = function(context,
                                    isServer,
                                    requestCert,
                                    rejectUnauthorized) {
  var pair = new SecurePair(context,
                            isServer,
                            requestCert,
                            rejectUnauthorized);
  return pair;
};


SecurePair.prototype.maybeInitFinished = function() {
  if (this.ssl && !this._secureEstablished && this.ssl.isInitFinished()) {
    if (process.features.tls_npn) {
      this.npnProtocol = this.ssl.getNegotiatedProtocol();
    }

    if (process.features.tls_sni) {
      this.servername = this.ssl.getServername();
    }

    this._secureEstablished = true;
    debug('secure established');
    this.emit('secure');
  }
};


SecurePair.prototype.destroy = function() {
  if (this._destroying) return;

  if (!this._doneFlag) {
    debug('SecurePair.destroy');
    this._destroying = true;

    // SecurePair should be destroyed only after it's streams
    this.cleartext.destroy();
    this.encrypted.destroy();

    this._doneFlag = true;
    this.ssl.error = null;
    this.ssl.close();
    this.ssl = null;
  }
};


SecurePair.prototype.error = function(returnOnly) {
  var err = this.ssl.error;
  this.ssl.error = null;

  if (!this._secureEstablished) {
    // Emit ECONNRESET instead of zero return
    if (!err || err.message === 'ZERO_RETURN') {
      var connReset = new Error('socket hang up');
      connReset.code = 'ECONNRESET';
      connReset.sslError = err && err.message;

      err = connReset;
    }
    this.destroy();
    if (!returnOnly) this.emit('error', err);
  } else if (this._isServer &&
             this._rejectUnauthorized &&
             /peer did not return a certificate/.test(err.message)) {
    // Not really an error.
    this.destroy();
  } else {
    if (!returnOnly) this.cleartext.emit('error', err);
  }
  return err;
};


exports.pipe = function pipe(pair, socket) {
  pair.encrypted.pipe(socket);
  socket.pipe(pair.encrypted);

  pair.encrypted.on('close', function() {
    process.nextTick(function() {
      // Encrypted should be unpiped from socket to prevent possible
      // write after destroy.
      pair.encrypted.unpipe(socket);
      socket.destroySoon();
    });
  });

  pair.fd = socket.fd;
  var cleartext = pair.cleartext;
  cleartext.socket = socket;
  cleartext.encrypted = pair.encrypted;
  cleartext.authorized = false;

  // cycle the data whenever the socket drains, so that
  // we can pull some more into it.  normally this would
  // be handled by the fact that pipe() triggers read() calls
  // on writable.drain, but CryptoStreams are a bit more
  // complicated.  Since the encrypted side actually gets
  // its data from the cleartext side, we have to give it a
  // light kick to get in motion again.
  socket.on('drain', function() {
    if (pair.encrypted._pending)
      pair.encrypted._writePending();
    if (pair.cleartext._pending)
      pair.cleartext._writePending();
    pair.encrypted.read(0);
    pair.cleartext.read(0);
  });

  function onerror(e) {
    if (cleartext._controlReleased) {
      cleartext.emit('error', e);
    }
  }

  function onclose() {
    socket.removeListener('error', onerror);
    socket.removeListener('timeout', ontimeout);
  }

  function ontimeout() {
    cleartext.emit('timeout');
  }

  socket.on('error', onerror);
  socket.on('close', onclose);
  socket.on('timeout', ontimeout);

  return cleartext;
};
