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

// Note: In 0.8 and before, crypto functions all defaulted to using
// binary-encoded strings rather than buffers.

exports.DEFAULT_ENCODING = 'buffer';

try {
  var binding = process.binding('crypto');
  var SecureContext = binding.SecureContext;
  var randomBytes = binding.randomBytes;
  var pseudoRandomBytes = binding.pseudoRandomBytes;
  var getCiphers = binding.getCiphers;
  var getHashes = binding.getHashes;
} catch (e) {
  throw new Error('node.js not compiled with openssl crypto support.');
}

var constants = require('constants');
var stream = require('stream');
var util = require('util');

// This is here because many functions accepted binary strings without
// any explicit encoding in older versions of node, and we don't want
// to break them unnecessarily.
function toBuf(str, encoding) {
  encoding = encoding || 'binary';
  if (util.isString(str)) {
    if (encoding === 'buffer')
      encoding = 'binary';
    str = new Buffer(str, encoding);
  }
  return str;
}


var assert = require('assert');
var StringDecoder = require('string_decoder').StringDecoder;

function Credentials(secureProtocol, flags, context) {
  if (!(this instanceof Credentials)) {
    return new Credentials(secureProtocol, flags, context);
  }

  if (context) {
    this.context = context;
  } else {
    this.context = new SecureContext();

    if (secureProtocol) {
      this.context.init(secureProtocol);
    } else {
      this.context.init();
    }
  }

  if (flags) this.context.setOptions(flags);
}

exports.Credentials = Credentials;


exports.createCredentials = function(options, context) {
  if (!options) options = {};

  var c = new Credentials(options.secureProtocol,
                          options.secureOptions,
                          context);

  if (context) return c;

  if (options.key) {
    if (options.passphrase) {
      c.context.setKey(options.key, options.passphrase);
    } else {
      c.context.setKey(options.key);
    }
  }

  if (options.cert) c.context.setCert(options.cert);

  if (options.ciphers) c.context.setCiphers(options.ciphers);

  if (options.ecdhCurve) c.context.setECDHCurve(options.ecdhCurve);

  if (options.ca) {
    if (util.isArray(options.ca)) {
      for (var i = 0, len = options.ca.length; i < len; i++) {
        c.context.addCACert(options.ca[i]);
      }
    } else {
      c.context.addCACert(options.ca);
    }
  } else {
    c.context.addRootCerts();
  }

  if (options.crl) {
    if (util.isArray(options.crl)) {
      for (var i = 0, len = options.crl.length; i < len; i++) {
        c.context.addCRL(options.crl[i]);
      }
    } else {
      c.context.addCRL(options.crl);
    }
  }

  if (options.sessionIdContext) {
    c.context.setSessionIdContext(options.sessionIdContext);
  }

  if (options.pfx) {
    var pfx = options.pfx;
    var passphrase = options.passphrase;

    pfx = toBuf(pfx);
    if (passphrase)
      passphrase = toBuf(passphrase);

    if (passphrase) {
      c.context.loadPKCS12(pfx, passphrase);
    } else {
      c.context.loadPKCS12(pfx);
    }
  }

  return c;
};


function LazyTransform(options) {
  this._options = options;
}
util.inherits(LazyTransform, stream.Transform);

[
  '_readableState',
  '_writableState',
  '_transformState'
].forEach(function(prop, i, props) {
  Object.defineProperty(LazyTransform.prototype, prop, {
    get: function() {
      stream.Transform.call(this, this._options);
      this._writableState.decodeStrings = false;
      this._writableState.defaultEncoding = 'binary';
      return this[prop];
    },
    set: function(val) {
      Object.defineProperty(this, prop, {
        value: val,
        enumerable: true,
        configurable: true,
        writable: true
      });
    },
    configurable: true,
    enumerable: true
  });
});


exports.createHash = exports.Hash = Hash;
function Hash(algorithm, options) {
  if (!(this instanceof Hash))
    return new Hash(algorithm, options);
  this._binding = new binding.Hash(algorithm);
  LazyTransform.call(this, options);
}

util.inherits(Hash, LazyTransform);

Hash.prototype._transform = function(chunk, encoding, callback) {
  this._binding.update(chunk, encoding);
  callback();
};

Hash.prototype._flush = function(callback) {
  var encoding = this._readableState.encoding || 'buffer';
  this.push(this._binding.digest(encoding), encoding);
  callback();
};

Hash.prototype.update = function(data, encoding) {
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding === 'buffer' && util.isString(data))
    encoding = 'binary';
  this._binding.update(data, encoding);
  return this;
};


Hash.prototype.digest = function(outputEncoding) {
  outputEncoding = outputEncoding || exports.DEFAULT_ENCODING;
  return this._binding.digest(outputEncoding);
};


exports.createHmac = exports.Hmac = Hmac;

function Hmac(hmac, key, options) {
  if (!(this instanceof Hmac))
    return new Hmac(hmac, key, options);
  this._binding = new binding.Hmac();
  this._binding.init(hmac, toBuf(key));
  LazyTransform.call(this, options);
}

util.inherits(Hmac, LazyTransform);

Hmac.prototype.update = Hash.prototype.update;
Hmac.prototype.digest = Hash.prototype.digest;
Hmac.prototype._flush = Hash.prototype._flush;
Hmac.prototype._transform = Hash.prototype._transform;


function getDecoder(decoder, encoding) {
  if (encoding === 'utf-8') encoding = 'utf8';  // Normalize encoding.
  decoder = decoder || new StringDecoder(encoding);
  assert(decoder.encoding === encoding, 'Cannot change encoding');
  return decoder;
}


exports.createCipher = exports.Cipher = Cipher;
function Cipher(cipher, password, options) {
  if (!(this instanceof Cipher))
    return new Cipher(cipher, password, options);
  this._binding = new binding.CipherBase(true);

  this._binding.init(cipher, toBuf(password));
  this._decoder = null;

  LazyTransform.call(this, options);
}

util.inherits(Cipher, LazyTransform);

Cipher.prototype._transform = function(chunk, encoding, callback) {
  this.push(this._binding.update(chunk, encoding));
  callback();
};

Cipher.prototype._flush = function(callback) {
  try {
    this.push(this._binding.final());
  } catch (e) {
    callback(e);
    return;
  }
  callback();
};

Cipher.prototype.update = function(data, inputEncoding, outputEncoding) {
  inputEncoding = inputEncoding || exports.DEFAULT_ENCODING;
  outputEncoding = outputEncoding || exports.DEFAULT_ENCODING;

  var ret = this._binding.update(data, inputEncoding);

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    ret = this._decoder.write(ret);
  }

  return ret;
};


Cipher.prototype.final = function(outputEncoding) {
  outputEncoding = outputEncoding || exports.DEFAULT_ENCODING;
  var ret = this._binding.final();

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    ret = this._decoder.end(ret);
  }

  return ret;
};


Cipher.prototype.setAutoPadding = function(ap) {
  this._binding.setAutoPadding(ap);
  return this;
};



exports.createCipheriv = exports.Cipheriv = Cipheriv;
function Cipheriv(cipher, key, iv, options) {
  if (!(this instanceof Cipheriv))
    return new Cipheriv(cipher, key, iv, options);
  this._binding = new binding.CipherBase(true);
  this._binding.initiv(cipher, toBuf(key), toBuf(iv));
  this._decoder = null;

  LazyTransform.call(this, options);
}

util.inherits(Cipheriv, LazyTransform);

Cipheriv.prototype._transform = Cipher.prototype._transform;
Cipheriv.prototype._flush = Cipher.prototype._flush;
Cipheriv.prototype.update = Cipher.prototype.update;
Cipheriv.prototype.final = Cipher.prototype.final;
Cipheriv.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;

Cipheriv.prototype.getAuthTag = function() {
  return this._binding.getAuthTag();
};


Cipheriv.prototype.setAuthTag = function(tagbuf) {
  this._binding.setAuthTag(tagbuf);
};



exports.createDecipher = exports.Decipher = Decipher;
function Decipher(cipher, password, options) {
  if (!(this instanceof Decipher))
    return new Decipher(cipher, password, options);

  this._binding = new binding.CipherBase(false);
  this._binding.init(cipher, toBuf(password));
  this._decoder = null;

  LazyTransform.call(this, options);
}

util.inherits(Decipher, LazyTransform);

Decipher.prototype._transform = Cipher.prototype._transform;
Decipher.prototype._flush = Cipher.prototype._flush;
Decipher.prototype.update = Cipher.prototype.update;
Decipher.prototype.final = Cipher.prototype.final;
Decipher.prototype.finaltol = Cipher.prototype.final;
Decipher.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;



exports.createDecipheriv = exports.Decipheriv = Decipheriv;
function Decipheriv(cipher, key, iv, options) {
  if (!(this instanceof Decipheriv))
    return new Decipheriv(cipher, key, iv, options);

  this._binding = new binding.CipherBase(false);
  this._binding.initiv(cipher, toBuf(key), toBuf(iv));
  this._decoder = null;

  LazyTransform.call(this, options);
}

util.inherits(Decipheriv, LazyTransform);

Decipheriv.prototype._transform = Cipher.prototype._transform;
Decipheriv.prototype._flush = Cipher.prototype._flush;
Decipheriv.prototype.update = Cipher.prototype.update;
Decipheriv.prototype.final = Cipher.prototype.final;
Decipheriv.prototype.finaltol = Cipher.prototype.final;
Decipheriv.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;
Decipheriv.prototype.getAuthTag = Cipheriv.prototype.getAuthTag;
Decipheriv.prototype.setAuthTag = Cipheriv.prototype.setAuthTag;



exports.createSign = exports.Sign = Sign;
function Sign(algorithm, options) {
  if (!(this instanceof Sign))
    return new Sign(algorithm, options);
  this._binding = new binding.Sign();
  this._binding.init(algorithm);

  stream.Writable.call(this, options);
}

util.inherits(Sign, stream.Writable);

Sign.prototype._write = function(chunk, encoding, callback) {
  this._binding.update(chunk, encoding);
  callback();
};

Sign.prototype.update = Hash.prototype.update;

Sign.prototype.sign = function(options, encoding) {
  if (!options)
    throw new Error('No key provided to sign');

  var key = options.key || options;
  var passphrase = options.passphrase || null;
  var ret = this._binding.sign(toBuf(key), null, passphrase);

  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    ret = ret.toString(encoding);

  return ret;
};



exports.createVerify = exports.Verify = Verify;
function Verify(algorithm, options) {
  if (!(this instanceof Verify))
    return new Verify(algorithm, options);

  this._binding = new binding.Verify;
  this._binding.init(algorithm);

  stream.Writable.call(this, options);
}

util.inherits(Verify, stream.Writable);

Verify.prototype._write = Sign.prototype._write;
Verify.prototype.update = Sign.prototype.update;

Verify.prototype.verify = function(object, signature, sigEncoding) {
  sigEncoding = sigEncoding || exports.DEFAULT_ENCODING;
  return this._binding.verify(toBuf(object), toBuf(signature, sigEncoding));
};



exports.createDiffieHellman = exports.DiffieHellman = DiffieHellman;

function DiffieHellman(sizeOrKey, encoding) {
  if (!(this instanceof DiffieHellman))
    return new DiffieHellman(sizeOrKey, encoding);

  if (!sizeOrKey)
    this._binding = new binding.DiffieHellman();
  else {
    encoding = encoding || exports.DEFAULT_ENCODING;
    sizeOrKey = toBuf(sizeOrKey, encoding);
    this._binding = new binding.DiffieHellman(sizeOrKey);
  }
}


exports.DiffieHellmanGroup =
    exports.createDiffieHellmanGroup =
    exports.getDiffieHellman = DiffieHellmanGroup;

function DiffieHellmanGroup(name) {
  if (!(this instanceof DiffieHellmanGroup))
    return new DiffieHellmanGroup(name);
  this._binding = new binding.DiffieHellmanGroup(name);
}


DiffieHellmanGroup.prototype.generateKeys =
    DiffieHellman.prototype.generateKeys =
    dhGenerateKeys;

function dhGenerateKeys(encoding) {
  var keys = this._binding.generateKeys();
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    keys = keys.toString(encoding);
  return keys;
}


DiffieHellmanGroup.prototype.computeSecret =
    DiffieHellman.prototype.computeSecret =
    dhComputeSecret;

function dhComputeSecret(key, inEnc, outEnc) {
  inEnc = inEnc || exports.DEFAULT_ENCODING;
  outEnc = outEnc || exports.DEFAULT_ENCODING;
  var ret = this._binding.computeSecret(toBuf(key, inEnc));
  if (outEnc && outEnc !== 'buffer')
    ret = ret.toString(outEnc);
  return ret;
}


DiffieHellmanGroup.prototype.getPrime =
    DiffieHellman.prototype.getPrime =
    dhGetPrime;

function dhGetPrime(encoding) {
  var prime = this._binding.getPrime();
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    prime = prime.toString(encoding);
  return prime;
}


DiffieHellmanGroup.prototype.getGenerator =
    DiffieHellman.prototype.getGenerator =
    dhGetGenerator;

function dhGetGenerator(encoding) {
  var generator = this._binding.getGenerator();
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    generator = generator.toString(encoding);
  return generator;
}


DiffieHellmanGroup.prototype.getPublicKey =
    DiffieHellman.prototype.getPublicKey =
    dhGetPublicKey;

function dhGetPublicKey(encoding) {
  var key = this._binding.getPublicKey();
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
}


DiffieHellmanGroup.prototype.getPrivateKey =
    DiffieHellman.prototype.getPrivateKey =
    dhGetPrivateKey;

function dhGetPrivateKey(encoding) {
  var key = this._binding.getPrivateKey();
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
}


DiffieHellman.prototype.setPublicKey = function(key, encoding) {
  encoding = encoding || exports.DEFAULT_ENCODING;
  this._binding.setPublicKey(toBuf(key, encoding));
  return this;
};


DiffieHellman.prototype.setPrivateKey = function(key, encoding) {
  encoding = encoding || exports.DEFAULT_ENCODING;
  this._binding.setPrivateKey(toBuf(key, encoding));
  return this;
};



exports.pbkdf2 = function(password, salt, iterations, keylen, callback) {
  if (!util.isFunction(callback))
    throw new Error('No callback provided to pbkdf2');

  return pbkdf2(password, salt, iterations, keylen, callback);
};


exports.pbkdf2Sync = function(password, salt, iterations, keylen) {
  return pbkdf2(password, salt, iterations, keylen);
};


function pbkdf2(password, salt, iterations, keylen, callback) {
  password = toBuf(password);
  salt = toBuf(salt);

  if (exports.DEFAULT_ENCODING === 'buffer')
    return binding.PBKDF2(password, salt, iterations, keylen, callback);

  // at this point, we need to handle encodings.
  var encoding = exports.DEFAULT_ENCODING;
  if (callback) {
    binding.PBKDF2(password, salt, iterations, keylen, function(er, ret) {
      if (ret)
        ret = ret.toString(encoding);
      callback(er, ret);
    });
  } else {
    var ret = binding.PBKDF2(password, salt, iterations, keylen);
    return ret.toString(encoding);
  }
}


exports.Certificate = Certificate;

function Certificate() {
  if (!(this instanceof Certificate))
    return new Certificate();

  this._binding = new binding.Certificate();
}


Certificate.prototype.verifySpkac = function(object) {
  return this._binding.verifySpkac(object);
};


Certificate.prototype.exportPublicKey = function(object, encoding) {
  return this._binding.exportPublicKey(toBuf(object, encoding));
};


Certificate.prototype.exportChallenge = function(object, encoding) {
  return this._binding.exportChallenge(toBuf(object, encoding));
};


exports.setEngine = function setEngine(id, flags) {
  if (!util.isString(id))
    throw new TypeError('id should be a string');

  if (flags && !util.isNumber(flags))
    throw new TypeError('flags should be a number, if present');
  flags = flags >>> 0;

  // Use provided engine for everything by default
  if (flags === 0)
    flags = constants.ENGINE_METHOD_ALL;

  return binding.setEngine(id, flags);
};

exports.randomBytes = randomBytes;
exports.pseudoRandomBytes = pseudoRandomBytes;

exports.rng = randomBytes;
exports.prng = pseudoRandomBytes;


exports.getCiphers = function() {
  return filterDuplicates(getCiphers.call(null, arguments));
};


exports.getHashes = function() {
  return filterDuplicates(getHashes.call(null, arguments));

};


function filterDuplicates(names) {
  // Drop all-caps names in favor of their lowercase aliases,
  // for example, 'sha1' instead of 'SHA1'.
  var ctx = {};
  names.forEach(function(name) {
    if (/^[0-9A-Z\-]+$/.test(name)) name = name.toLowerCase();
    ctx[name] = true;
  });
  return Object.getOwnPropertyNames(ctx).sort();
}
