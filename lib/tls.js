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

var crypto = require('crypto');
var util = require('util');
var net = require('net');
var url = require('url');
var events = require('events');
var stream = require('stream');
var assert = require('assert').ok;
var constants = require('constants');

var DEFAULT_CIPHERS = 'ECDHE-RSA-AES128-SHA256:AES128-GCM-SHA256:' + // TLS 1.2
                      'RC4:HIGH:!MD5:!aNULL:!EDH';                   // TLS 1.0

// Allow {CLIENT_RENEG_LIMIT} client-initiated session renegotiations
// every {CLIENT_RENEG_WINDOW} seconds. An error event is emitted if more
// renegotations are seen. The settings are applied to all remote client
// connections.
exports.CLIENT_RENEG_LIMIT = 3;
exports.CLIENT_RENEG_WINDOW = 600;

exports.SLAB_BUFFER_SIZE = 10 * 1024 * 1024;

exports.getCiphers = function() {
  var names = process.binding('crypto').getSSLCiphers();
  // Drop all-caps names in favor of their lowercase aliases,
  var ctx = {};
  names.forEach(function(name) {
    if (/^[0-9A-Z\-]+$/.test(name)) name = name.toLowerCase();
    ctx[name] = true;
  });
  return Object.getOwnPropertyNames(ctx).sort();
};


var debug;
if (process.env.NODE_DEBUG && /tls/.test(process.env.NODE_DEBUG)) {
  debug = function(a) { console.error('TLS:', a); };
} else {
  debug = function() { };
}


var Connection = null;
try {
  Connection = process.binding('crypto').Connection;
} catch (e) {
  throw new Error('node.js not compiled with openssl crypto support.');
}

// Convert protocols array into valid OpenSSL protocols list
// ("\x06spdy/2\x08http/1.1\x08http/1.0")
function convertNPNProtocols(NPNProtocols, out) {
  // If NPNProtocols is Array - translate it into buffer
  if (Array.isArray(NPNProtocols)) {
    var buff = new Buffer(NPNProtocols.reduce(function(p, c) {
      return p + 1 + Buffer.byteLength(c);
    }, 0));

    NPNProtocols.reduce(function(offset, c) {
      var clen = Buffer.byteLength(c);
      buff[offset] = clen;
      buff.write(c, offset + 1);

      return offset + 1 + clen;
    }, 0);

    NPNProtocols = buff;
  }

  // If it's already a Buffer - store it
  if (Buffer.isBuffer(NPNProtocols)) {
    out.NPNProtocols = NPNProtocols;
  }
}


function checkServerIdentity(host, cert) {
  // Create regexp to much hostnames
  function regexpify(host, wildcards) {
    // Add trailing dot (make hostnames uniform)
    if (!/\.$/.test(host)) host += '.';

    // The same applies to hostname with more than one wildcard,
    // if hostname has wildcard when wildcards are not allowed,
    // or if there are less than two dots after wildcard (i.e. *.com or *d.com)
    //
    // also
    //
    // "The client SHOULD NOT attempt to match a presented identifier in
    // which the wildcard character comprises a label other than the
    // left-most label (e.g., do not match bar.*.example.net)."
    // RFC6125
    if (!wildcards && /\*/.test(host) || /[\.\*].*\*/.test(host) ||
        /\*/.test(host) && !/\*.*\..+\..+/.test(host)) {
      return /$./;
    }

    // Replace wildcard chars with regexp's wildcard and
    // escape all characters that have special meaning in regexps
    // (i.e. '.', '[', '{', '*', and others)
    var re = host.replace(
        /\*([a-z0-9\\-_\.])|[\.,\-\\\^\$+?*\[\]\(\):!\|{}]/g,
        function(all, sub) {
          if (sub) return '[a-z0-9\\-_]*' + (sub === '-' ? '\\-' : sub);
          return '\\' + all;
        });

    return new RegExp('^' + re + '$', 'i');
  }

  var dnsNames = [],
      uriNames = [],
      ips = [],
      matchCN = true,
      valid = false;

  // There're several names to perform check against:
  // CN and altnames in certificate extension
  // (DNS names, IP addresses, and URIs)
  //
  // Walk through altnames and generate lists of those names
  if (cert.subjectaltname) {
    cert.subjectaltname.split(/, /g).forEach(function(altname) {
      if (/^DNS:/.test(altname)) {
        dnsNames.push(altname.slice(4));
      } else if (/^IP Address:/.test(altname)) {
        ips.push(altname.slice(11));
      } else if (/^URI:/.test(altname)) {
        var uri = url.parse(altname.slice(4));
        if (uri) uriNames.push(uri.hostname);
      }
    });
  }

  // If hostname is an IP address, it should be present in the list of IP
  // addresses.
  if (net.isIP(host)) {
    valid = ips.some(function(ip) {
      return ip === host;
    });
  } else {
    // Transform hostname to canonical form
    if (!/\.$/.test(host)) host += '.';

    // Otherwise check all DNS/URI records from certificate
    // (with allowed wildcards)
    dnsNames = dnsNames.map(function(name) {
      return regexpify(name, true);
    });

    // Wildcards ain't allowed in URI names
    uriNames = uriNames.map(function(name) {
      return regexpify(name, false);
    });

    dnsNames = dnsNames.concat(uriNames);

    if (dnsNames.length > 0) matchCN = false;

    // Match against Common Name (CN) only if no supported identifiers are
    // present.
    //
    // "As noted, a client MUST NOT seek a match for a reference identifier
    //  of CN-ID if the presented identifiers include a DNS-ID, SRV-ID,
    //  URI-ID, or any application-specific identifier types supported by the
    //  client."
    // RFC6125
    if (matchCN) {
      var commonNames = cert.subject.CN;
      if (Array.isArray(commonNames)) {
        for (var i = 0, k = commonNames.length; i < k; ++i) {
          dnsNames.push(regexpify(commonNames[i], true));
        }
      } else {
        dnsNames.push(regexpify(commonNames, true));
      }
    }

    valid = dnsNames.some(function(re) {
      return re.test(host);
    });
  }

  return valid;
}
exports.checkServerIdentity = checkServerIdentity;


function SlabBuffer() {
  this.create();
}


SlabBuffer.prototype.create = function create() {
  this.isFull = false;
  this.pool = new Buffer(exports.SLAB_BUFFER_SIZE);
  this.offset = 0;
  this.remaining = this.pool.length;
};


SlabBuffer.prototype.use = function use(context, fn, size) {
  if (this.remaining === 0) {
    this.isFull = true;
    return 0;
  }

  var actualSize = this.remaining;

  if (size !== null) actualSize = Math.min(size, actualSize);

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
  this._resumingSession = false;
  this._reading = true;
  this._destroyed = false;
  this._ended = false;
  this._finished = false;
  this._opposite = null;

  if (slabBuffer === null) slabBuffer = new SlabBuffer();
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
        this.pair.ssl.shutdown();
      }
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

  if (this.onend) this.onend();
}


CryptoStream.prototype._write = function write(data, encoding, cb) {
  assert(this._pending === null);

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
      debug('cleartext.write called with ' + data.length + ' bytes');
      written = this.pair.ssl.clearIn(data, 0, data.length);
    } else {
      debug('encrypted.write called with ' + data.length + ' bytes');
      written = this.pair.ssl.encIn(data, 0, data.length);
    }

    // Handle and report errors
    if (this.pair.ssl && this.pair.ssl.error) {
      return cb(this.pair.error(true));
    }

    // Force SSL_read call to cycle some states/data inside OpenSSL
    this.pair.cleartext.read(0);

    // Cycle encrypted data
    if (this.pair.encrypted._internallyPendingBytes()) {
      this.pair.encrypted.read(0);
    }

    // Get NPN and Server name when ready
    this.pair.maybeInitFinished();

    // Whole buffer was written
    if (written === data.length) {
      if (this === this.pair.cleartext) {
        debug('cleartext.write succeed with ' + data.length + ' bytes');
      } else {
        debug('encrypted.write succeed with ' + data.length + ' bytes');
      }

      return cb(null);
    }
    if (written !== 0 && written !== -1) {
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
    debug('cleartext.write queued with ' + data.length + ' bytes');
  } else {
    debug('encrypted.write queued with ' + data.length + ' bytes');
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
    debug('cleartext.read called with ' + size + ' bytes');
    out = this.pair.ssl.clearOut;
  } else {
    debug('encrypted.read called with ' + size + ' bytes');
    out = this.pair.ssl.encOut;
  }

  var bytesRead = 0,
      start = this._buffer.offset;
  do {
    var read = this._buffer.use(this.pair.ssl, out, size);
    if (read > 0) {
      bytesRead += read;
      size -= read;
    }

    // Handle and report errors
    if (this.pair.ssl && this.pair.ssl.error) {
      this.pair.error();
      break;
    }

    // Get NPN and Server name when ready
    this.pair.maybeInitFinished();
  } while (read > 0 && !this._buffer.isFull && bytesRead < size);

  // Create new buffer if previous was filled up
  var pool = this._buffer.pool;
  if (this._buffer.isFull) this._buffer.create();

  assert(bytesRead >= 0);

  if (this === this.pair.cleartext) {
    debug('cleartext.read succeed with ' + bytesRead + ' bytes');
  } else {
    debug('encrypted.read succeed with ' + bytesRead + ' bytes');
  }

  // Try writing pending data
  if (this._pending !== null) this._writePending();
  if (this._opposite._pending !== null) this._opposite._writePending();

  if (bytesRead === 0) {
    // EOF when cleartext has finished and we have nothing to read
    if (this._opposite._finished && this._internallyPendingBytes() === 0) {
      // Perform graceful shutdown
      this._done();

      // No half-open, sorry!
      if (this === this.pair.cleartext)
        this._opposite._done();

      // EOF
      return this.push(null);
    }

    // Bail out
    return this.push('');
  }

  // Give them requested data
  if (this.ondata) {
    var self = this;
    this.ondata(pool, start, start + bytesRead);

    // Consume data automatically
    // simple/test-https-drain fails without it
    process.nextTick(function() {
      self.read(bytesRead);
    });
  }
  return this.push(pool.slice(start, start + bytesRead));
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


// Example:
// C=US\nST=CA\nL=SF\nO=Joyent\nOU=Node.js\nCN=ca1\nemailAddress=ry@clouds.org
function parseCertString(s) {
  var out = {};
  var parts = s.split('\n');
  for (var i = 0, len = parts.length; i < len; i++) {
    var sepIndex = parts[i].indexOf('=');
    if (sepIndex > 0) {
      var key = parts[i].slice(0, sepIndex);
      var value = parts[i].slice(sepIndex + 1);
      if (key in out) {
        if (!Array.isArray(out[key])) {
          out[key] = [out[key]];
        }
        out[key].push(value);
      } else {
        out[key] = value;
      }
    }
  }
  return out;
}


CryptoStream.prototype.getPeerCertificate = function() {
  if (this.pair.ssl) {
    var c = this.pair.ssl.getPeerCertificate();

    if (c) {
      if (c.issuer) c.issuer = parseCertString(c.issuer);
      if (c.subject) c.subject = parseCertString(c.subject);
      return c;
    }
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
  if (this._pending !== null) this._writePending();

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

  if (this._writableState.finished)
    this.destroy();
  else
    this.once('finish', this.destroy);
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


CleartextStream.prototype.__defineGetter__('remotePort', function() {
  return this.socket && this.socket.remotePort;
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
  var now = Date.now();

  assert(now >= ssl.lastHandshakeTime);

  if ((now - ssl.lastHandshakeTime) >= exports.CLIENT_RENEG_WINDOW * 1000) {
    ssl.handshakes = 0;
  }

  var first = (ssl.lastHandshakeTime === 0);
  ssl.lastHandshakeTime = now;
  if (first) return;

  if (++ssl.handshakes > exports.CLIENT_RENEG_LIMIT) {
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
  this.server.emit('newSession', key, session);
}


/**
 * Provides a pair of streams to do encrypted communication.
 */

function SecurePair(credentials, isServer, requestCert, rejectUnauthorized,
                    options) {
  if (!(this instanceof SecurePair)) {
    return new SecurePair(credentials,
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

  process.nextTick(function() {
    /* The Connection may be destroyed by an abort call */
    if (self.ssl) {
      self.ssl.start();
    }
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
    ciphers: self.ciphers || DEFAULT_CIPHERS,
    secureProtocol: self.secureProtocol,
    secureOptions: self.secureOptions,
    crl: self.crl,
    sessionIdContext: self.sessionIdContext
  });

  var timeout = options.handshakeTimeout || (120 * 1000);

  if (typeof timeout !== 'number') {
    throw new TypeError('handshakeTimeout must be a number');
  }

  // constructor call
  net.Server.call(this, function(socket) {
    var creds = crypto.createCredentials(null, sharedCreds.context);

    var pair = new SecurePair(creds,
                              true,
                              self.requestCert,
                              self.rejectUnauthorized,
                              {
                                server: self,
                                NPNProtocols: self.NPNProtocols,
                                SNICallback: self.SNICallback,

                                // Stream options
                                cleartext: self._cleartext,
                                encrypted: self._encrypted
                              });

    var cleartext = pipe(pair, socket);
    cleartext._controlReleased = false;

    function listener() {
      pair.emit('error', new Error('TLS handshake timeout'));
    }

    if (timeout > 0) {
      socket.setTimeout(timeout, listener);
    }

    pair.once('secure', function() {
      socket.setTimeout(0, listener);

      pair.cleartext.authorized = false;
      pair.cleartext.npnProtocol = pair.npnProtocol;
      pair.cleartext.servername = pair.servername;

      if (!self.requestCert) {
        cleartext._controlReleased = true;
        self.emit('secureConnection', pair.cleartext, pair.encrypted);
      } else {
        var verifyError = pair.ssl.verifyError();
        if (verifyError) {
          pair.cleartext.authorizationError = verifyError.message;

          if (self.rejectUnauthorized) {
            socket.destroy();
            pair.destroy();
          } else {
            cleartext._controlReleased = true;
            self.emit('secureConnection', pair.cleartext, pair.encrypted);
          }
        } else {
          pair.cleartext.authorized = true;
          cleartext._controlReleased = true;
          self.emit('secureConnection', pair.cleartext, pair.encrypted);
        }
      }
    });
    pair.on('error', function(err) {
      self.emit('clientError', err, this);
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

  if (options.pfx) this.pfx = options.pfx;
  if (options.key) this.key = options.key;
  if (options.passphrase) this.passphrase = options.passphrase;
  if (options.cert) this.cert = options.cert;
  if (options.ca) this.ca = options.ca;
  if (options.secureProtocol) this.secureProtocol = options.secureProtocol;
  if (options.crl) this.crl = options.crl;
  if (options.ciphers) this.ciphers = options.ciphers;
  var secureOptions = options.secureOptions || 0;
  if (options.honorCipherOrder) {
    secureOptions |= constants.SSL_OP_CIPHER_SERVER_PREFERENCE;
  }
  if (secureOptions) this.secureOptions = secureOptions;
  if (options.NPNProtocols) convertNPNProtocols(options.NPNProtocols, this);
  if (options.SNICallback) {
    this.SNICallback = options.SNICallback;
  } else {
    this.SNICallback = this.SNICallback.bind(this);
  }
  if (options.sessionIdContext) {
    this.sessionIdContext = options.sessionIdContext;
  } else if (this.requestCert) {
    this.sessionIdContext = crypto.createHash('md5')
                                  .update(process.argv.join(' '))
                                  .digest('hex');
  }
  if (options.cleartext) this.cleartext = options.cleartext;
  if (options.encrypted) this.encrypted = options.encrypted;
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

Server.prototype.SNICallback = function(servername) {
  var ctx;

  this._contexts.some(function(elem) {
    if (servername.match(elem[0]) !== null) {
      ctx = elem[1];
      return true;
    }
  });

  return ctx;
};


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

  if (typeof listArgs[1] === 'object') {
    options = util._extend(options, listArgs[1]);
  } else if (typeof listArgs[2] === 'object') {
    options = util._extend(options, listArgs[2]);
  }

  return (cb) ? [options, cb] : [options];
}

exports.connect = function(/* [port, host], options, cb */) {
  var args = normalizeConnectArgs(arguments);
  var options = args[0];
  var cb = args[1];

  var defaults = {
    rejectUnauthorized: '0' !== process.env.NODE_TLS_REJECT_UNAUTHORIZED
  };
  options = util._extend(defaults, options || {});

  var socket = options.socket ? options.socket : new net.Stream();

  var sslcontext = crypto.createCredentials(options);

  convertNPNProtocols(options.NPNProtocols, this);
  var hostname = options.servername || options.host || 'localhost',
      pair = new SecurePair(sslcontext, false, true,
                            options.rejectUnauthorized === true ? true : false,
                            {
                              NPNProtocols: this.NPNProtocols,
                              servername: hostname,
                              cleartext: options.cleartext,
                              encrypted: options.encrypted
                            });

  if (options.session) {
    var session = options.session;
    if (typeof session === 'string')
      session = new Buffer(session, 'binary');
    pair.ssl.setSession(session);
  }

  var cleartext = pipe(pair, socket);
  if (cb) {
    cleartext.once('secureConnect', cb);
  }

  if (!options.socket) {
    var connect_opt = (options.path && !options.port) ? {path: options.path} : {
      port: options.port,
      host: options.host,
      localAddress: options.localAddress
    };
    socket.connect(connect_opt);
  }

  pair.on('secure', function() {
    var verifyError = pair.ssl.verifyError();

    cleartext.npnProtocol = pair.npnProtocol;

    // Verify that server's identity matches it's certificate's names
    if (!verifyError) {
      var validCert = checkServerIdentity(hostname,
                                          pair.cleartext.getPeerCertificate());
      if (!validCert) {
        verifyError = new Error('Hostname/IP doesn\'t match certificate\'s ' +
                                'altnames');
      }
    }

    if (verifyError) {
      cleartext.authorized = false;
      cleartext.authorizationError = verifyError.message;

      if (pair._rejectUnauthorized) {
        cleartext.emit('error', verifyError);
        pair.destroy();
      } else {
        cleartext.emit('secureConnect');
      }
    } else {
      cleartext.authorized = true;
      cleartext.emit('secureConnect');
    }
  });
  pair.on('error', function(err) {
    cleartext.emit('error', err);
  });

  cleartext._controlReleased = true;
  return cleartext;
};


function pipe(pair, socket) {
  pair.encrypted.pipe(socket);
  socket.pipe(pair.encrypted);

  pair.encrypted.on('close', function() {
    process.nextTick(function() {
      socket.destroy();
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
}
