var util = require('util');
var events = require('events');
var stream = require('stream');
var assert = process.assert;

var debugLevel = parseInt(process.env.NODE_DEBUG, 16);

function debug () {
  if (debugLevel & 0x2) {
    util.error.apply(this, arguments);
  }
}

/* Lazy Loaded crypto object */
var SecureStream = null;

/**
 * Provides a pair of streams to do encrypted communication.
 */

function SecurePair(credentials, isServer) {
  if (!(this instanceof SecurePair)) {
    return new SecurePair(credentials, isServer);
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

  var crypto = require("crypto");

  if (!credentials) {
    this.credentials = crypto.createCredentials();
  } else {
    this.credentials = credentials;
  }

  if (!this._isServer) {
    /* For clients, we will always have either a given ca list or be using default one */
    this.credentials.shouldVerify = true;
  }

  this._secureEstablished = false;
  this._encInPending = [];
  this._clearInPending = [];

  this._ssl = new SecureStream(this.credentials.context,
                               this._isServer ? true : false,
                               this.credentials.shouldVerify);


  /* Acts as a r/w stream to the cleartext side of the stream. */
  this.cleartext = new stream.Stream();
  this.cleartext.readable = true;
  /* Acts as a r/w stream to the encrypted side of the stream. */
  this.encrypted = new stream.Stream();
  this.encrypted.readable = true;

  this.cleartext.write = function(data) {
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

  this.cleartext.on('end', function(err) {
    debug('clearIn end');
    if (!self._done) {
      self._ssl.shutdown();
      self._cycle();
    }
    self._destroy(err);
  });

  this.cleartext.on('close', function() {
    debug('source close');
    self.emit('close');
    self._destroy();
  });

  this.encrypted.on('end', function() {
    if (!self._done) {
      self._error(new Error('Encrypted stream ended before secure pair was done'));
    }
  });

  this.encrypted.on('close', function() {
    if (!self._done) {
      self._error(new Error('Encrypted stream closed before secure pair was done'));
    }
  });

  this.cleartext.on('drain', function() {
    debug('source drain');
    self._cycle();
    self.encrypted.resume();
  });

  this.encrypted.on('drain', function() {
    debug('target drain');
    self._cycle();
    self.cleartext.resume();
  });

  process.nextTick(function() {
    self._ssl.start();
    self._cycle();
  });
}

util.inherits(SecurePair, events.EventEmitter);

exports.createSecurePair = function(credentials, isServer) {
  var pair = new SecurePair(credentials, isServer);
  return pair;
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
 * The four pipelines, using terminology of the client (server is just reversed):
 *   1) Encrypted Output stream (Writing encrypted data to peer)
 *   2) Encrypted Input stream (Reading encrypted data from peer)
 *   3) Cleartext Output stream (Decrypted content from the peer)
 *   4) Cleartext Input stream (Cleartext content to send to the peer)
 *
 * This function attempts to pull any available data out of the Cleartext
 * input stream (#4), and the Encrypted input stream (#2).  Then it pushes
 * any data available from the cleartext output stream (#3), and finally
 * from the Encrypted output stream (#1)
 *
 * It is called whenever we do something with OpenSSL -- post reciving content,
 * trying to flush, trying to change ciphers, or shutting down the connection.
 *
 * Because it is also called everywhere, we also check if the connection
 * has completed negotiation and emit 'secure' from here if it has.
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
  var chunkBytes;
  var chunk = null;
  var pool = null;

  while (this._encInPending.length > 0) {
    tmp = this._encInPending.shift();

    try {
      debug('writng from encIn');
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

  function mover(reader, writer, checker) {
    var bytesRead;
    var pool;
    var chunkBytes;
    do {
      bytesRead = 0;
      chunkBytes = 0;
      pool = new Buffer(4096);
      pool.used = 0;

      do {
        try {
          chunkBytes = reader(pool,
                              pool.used + bytesRead,
                              pool.length - pool.used - bytesRead)
        } catch (e) {
          return self._error(e);
        }
        if (chunkBytes >= 0) {
          bytesRead += chunkBytes;
        }
      } while ((chunkBytes > 0) && (pool.used + bytesRead < pool.length));

      if (bytesRead > 0) {
        chunk = pool.slice(0, bytesRead);
        writer(chunk);
      }
    } while (checker(bytesRead));
  }

  mover(
    function(pool, offset, length) {
      debug('reading from clearOut');
      return self._ssl.clearOut(pool, offset, length);
    },
    function(chunk) {
      self.cleartext.emit('data', chunk);
    },
    function(bytesRead) {
      return bytesRead > 0 && self._cleartextWriteState === true;
    });

  mover(
    function(pool, offset, length) {
      debug('reading from encOut');
      return self._ssl.encOut(pool, offset, length);
    },
    function(chunk) {
      self.encrypted.emit('data', chunk);
    },
    function(bytesRead) {
      return bytesRead > 0 && self._encryptedWriteState === true;
    });

  if (!this._secureEstablished && this._ssl.isInitFinished()) {
    this._secureEstablished = true;
    debug('secure established');
    this.emit('secure');
    this._cycle();
  }
};

SecurePair.prototype._destroy = function(err)
{
  if (!this._done) {
    this._done = true;
    this._ssl.close();
    delete this._ssl;
    this.emit('end', err);
  }
};

SecurePair.prototype._error = function (err)
{
  this.emit('error', err);
};

SecurePair.prototype.getPeerCertificate = function (err)
{
  return this._ssl.getPeerCertificate();
};

SecurePair.prototype.getCipher = function (err)
{
  return this._ssl.getCurrentCipher();
};
