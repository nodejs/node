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

var DH_GENERATOR = 2;

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
exports._toBuf = toBuf;


var assert = require('assert');
var StringDecoder = require('string_decoder').StringDecoder;


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
  this._handle = new binding.Hash(algorithm);
  LazyTransform.call(this, options);
}

util.inherits(Hash, LazyTransform);

Hash.prototype._transform = function(chunk, encoding, callback) {
  this._handle.update(chunk, encoding);
  callback();
};

Hash.prototype._flush = function(callback) {
  var encoding = this._readableState.encoding || 'buffer';
  this.push(this._handle.digest(encoding), encoding);
  callback();
};

Hash.prototype.update = function(data, encoding) {
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding === 'buffer' && util.isString(data))
    encoding = 'binary';
  this._handle.update(data, encoding);
  return this;
};


Hash.prototype.digest = function(outputEncoding) {
  outputEncoding = outputEncoding || exports.DEFAULT_ENCODING;
  return this._handle.digest(outputEncoding);
};


exports.createHmac = exports.Hmac = Hmac;

function Hmac(hmac, key, options) {
  if (!(this instanceof Hmac))
    return new Hmac(hmac, key, options);
  this._handle = new binding.Hmac();
  this._handle.init(hmac, toBuf(key));
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
  this._handle = new binding.CipherBase(true);

  this._handle.init(cipher, toBuf(password));
  this._decoder = null;

  LazyTransform.call(this, options);
}

util.inherits(Cipher, LazyTransform);

Cipher.prototype._transform = function(chunk, encoding, callback) {
  this.push(this._handle.update(chunk, encoding));
  callback();
};

Cipher.prototype._flush = function(callback) {
  try {
    this.push(this._handle.final());
  } catch (e) {
    callback(e);
    return;
  }
  callback();
};

Cipher.prototype.update = function(data, inputEncoding, outputEncoding) {
  inputEncoding = inputEncoding || exports.DEFAULT_ENCODING;
  outputEncoding = outputEncoding || exports.DEFAULT_ENCODING;

  var ret = this._handle.update(data, inputEncoding);

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    ret = this._decoder.write(ret);
  }

  return ret;
};


Cipher.prototype.final = function(outputEncoding) {
  outputEncoding = outputEncoding || exports.DEFAULT_ENCODING;
  var ret = this._handle.final();

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    ret = this._decoder.end(ret);
  }

  return ret;
};


Cipher.prototype.setAutoPadding = function(ap) {
  this._handle.setAutoPadding(ap);
  return this;
};



exports.createCipheriv = exports.Cipheriv = Cipheriv;
function Cipheriv(cipher, key, iv, options) {
  if (!(this instanceof Cipheriv))
    return new Cipheriv(cipher, key, iv, options);
  this._handle = new binding.CipherBase(true);
  this._handle.initiv(cipher, toBuf(key), toBuf(iv));
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
  return this._handle.getAuthTag();
};


Cipheriv.prototype.setAuthTag = function(tagbuf) {
  this._handle.setAuthTag(tagbuf);
};

Cipheriv.prototype.setAAD = function(aadbuf) {
  this._handle.setAAD(aadbuf);
};


exports.createDecipher = exports.Decipher = Decipher;
function Decipher(cipher, password, options) {
  if (!(this instanceof Decipher))
    return new Decipher(cipher, password, options);

  this._handle = new binding.CipherBase(false);
  this._handle.init(cipher, toBuf(password));
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

  this._handle = new binding.CipherBase(false);
  this._handle.initiv(cipher, toBuf(key), toBuf(iv));
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
Decipheriv.prototype.setAAD = Cipheriv.prototype.setAAD;



exports.createSign = exports.Sign = Sign;
function Sign(algorithm, options) {
  if (!(this instanceof Sign))
    return new Sign(algorithm, options);
  this._handle = new binding.Sign();
  this._handle.init(algorithm);

  stream.Writable.call(this, options);
}

util.inherits(Sign, stream.Writable);

Sign.prototype._write = function(chunk, encoding, callback) {
  this._handle.update(chunk, encoding);
  callback();
};

Sign.prototype.update = Hash.prototype.update;

Sign.prototype.sign = function(options, encoding) {
  if (!options)
    throw new Error('No key provided to sign');

  var key = options.key || options;
  var passphrase = options.passphrase || null;
  var ret = this._handle.sign(toBuf(key), null, passphrase);

  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    ret = ret.toString(encoding);

  return ret;
};



exports.createVerify = exports.Verify = Verify;
function Verify(algorithm, options) {
  if (!(this instanceof Verify))
    return new Verify(algorithm, options);

  this._handle = new binding.Verify;
  this._handle.init(algorithm);

  stream.Writable.call(this, options);
}

util.inherits(Verify, stream.Writable);

Verify.prototype._write = Sign.prototype._write;
Verify.prototype.update = Sign.prototype.update;

Verify.prototype.verify = function(object, signature, sigEncoding) {
  sigEncoding = sigEncoding || exports.DEFAULT_ENCODING;
  return this._handle.verify(toBuf(object), toBuf(signature, sigEncoding));
};

exports.publicEncrypt = function(options, buffer) {
  var key = options.key || options;
  var padding = options.padding || constants.RSA_PKCS1_OAEP_PADDING;
  return binding.publicEncrypt(toBuf(key), buffer, padding);
};

exports.privateDecrypt = function(options, buffer) {
  var key = options.key || options;
  var passphrase = options.passphrase || null;
  var padding = options.padding || constants.RSA_PKCS1_OAEP_PADDING;
  return binding.privateDecrypt(toBuf(key), buffer, padding, passphrase);
};



exports.createDiffieHellman = exports.DiffieHellman = DiffieHellman;

function DiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding) {
  if (!(this instanceof DiffieHellman))
    return new DiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding);

  if (!util.isBuffer(sizeOrKey) &&
      typeof sizeOrKey !== 'number' &&
      typeof sizeOrKey !== 'string')
    throw new TypeError('First argument should be number, string or Buffer');

  if (keyEncoding) {
    if (typeof keyEncoding !== 'string' ||
        (!Buffer.isEncoding(keyEncoding) && keyEncoding !== 'buffer')) {
      genEncoding = generator;
      generator = keyEncoding;
      keyEncoding = false;
    }
  }

  keyEncoding = keyEncoding || exports.DEFAULT_ENCODING;
  genEncoding = genEncoding || exports.DEFAULT_ENCODING;

  if (typeof sizeOrKey !== 'number')
    sizeOrKey = toBuf(sizeOrKey, keyEncoding);

  if (!generator)
    generator = DH_GENERATOR;
  else if (typeof generator !== 'number')
    generator = toBuf(generator, genEncoding);

  this._handle = new binding.DiffieHellman(sizeOrKey, generator);
  Object.defineProperty(this, 'verifyError', {
    enumerable: true,
    value: this._handle.verifyError,
    writable: false
  });
}


exports.DiffieHellmanGroup =
    exports.createDiffieHellmanGroup =
    exports.getDiffieHellman = DiffieHellmanGroup;

function DiffieHellmanGroup(name) {
  if (!(this instanceof DiffieHellmanGroup))
    return new DiffieHellmanGroup(name);
  this._handle = new binding.DiffieHellmanGroup(name);
  Object.defineProperty(this, 'verifyError', {
    enumerable: true,
    value: this._handle.verifyError,
    writable: false
  });
}


DiffieHellmanGroup.prototype.generateKeys =
    DiffieHellman.prototype.generateKeys =
    dhGenerateKeys;

function dhGenerateKeys(encoding) {
  var keys = this._handle.generateKeys();
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
  var ret = this._handle.computeSecret(toBuf(key, inEnc));
  if (outEnc && outEnc !== 'buffer')
    ret = ret.toString(outEnc);
  return ret;
}


DiffieHellmanGroup.prototype.getPrime =
    DiffieHellman.prototype.getPrime =
    dhGetPrime;

function dhGetPrime(encoding) {
  var prime = this._handle.getPrime();
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    prime = prime.toString(encoding);
  return prime;
}


DiffieHellmanGroup.prototype.getGenerator =
    DiffieHellman.prototype.getGenerator =
    dhGetGenerator;

function dhGetGenerator(encoding) {
  var generator = this._handle.getGenerator();
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    generator = generator.toString(encoding);
  return generator;
}


DiffieHellmanGroup.prototype.getPublicKey =
    DiffieHellman.prototype.getPublicKey =
    dhGetPublicKey;

function dhGetPublicKey(encoding) {
  var key = this._handle.getPublicKey();
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
}


DiffieHellmanGroup.prototype.getPrivateKey =
    DiffieHellman.prototype.getPrivateKey =
    dhGetPrivateKey;

function dhGetPrivateKey(encoding) {
  var key = this._handle.getPrivateKey();
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
}


DiffieHellman.prototype.setPublicKey = function(key, encoding) {
  encoding = encoding || exports.DEFAULT_ENCODING;
  this._handle.setPublicKey(toBuf(key, encoding));
  return this;
};


DiffieHellman.prototype.setPrivateKey = function(key, encoding) {
  encoding = encoding || exports.DEFAULT_ENCODING;
  this._handle.setPrivateKey(toBuf(key, encoding));
  return this;
};


function ECDH(curve) {
  if (!util.isString(curve))
    throw new TypeError('curve should be a string');

  this._handle = new binding.ECDH(curve);
}

exports.createECDH = function createECDH(curve) {
  return new ECDH(curve);
};

ECDH.prototype.computeSecret = DiffieHellman.prototype.computeSecret;
ECDH.prototype.setPrivateKey = DiffieHellman.prototype.setPrivateKey;
ECDH.prototype.setPublicKey = DiffieHellman.prototype.setPublicKey;
ECDH.prototype.getPrivateKey = DiffieHellman.prototype.getPrivateKey;

ECDH.prototype.generateKeys = function generateKeys(encoding, format) {
  this._handle.generateKeys();

  return this.getPublicKey(encoding, format);
};

ECDH.prototype.getPublicKey = function getPublicKey(encoding, format) {
  var f;
  if (format) {
    if (typeof format === 'number')
      f = format;
    if (format === 'compressed')
      f = constants.POINT_CONVERSION_COMPRESSED;
    else if (format === 'hybrid')
      f = constants.POINT_CONVERSION_HYBRID;
    // Default
    else if (format === 'uncompressed')
      f = constants.POINT_CONVERSION_UNCOMPRESSED;
    else
      throw TypeError('Bad format: ' + format);
  } else {
    f = constants.POINT_CONVERSION_UNCOMPRESSED;
  }
  var key = this._handle.getPublicKey(f);
  encoding = encoding || exports.DEFAULT_ENCODING;
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
};



exports.pbkdf2 = function(password,
                          salt,
                          iterations,
                          keylen,
                          digest,
                          callback) {
  if (util.isFunction(digest)) {
    callback = digest;
    digest = undefined;
  }

  if (!util.isFunction(callback))
    throw new Error('No callback provided to pbkdf2');

  return pbkdf2(password, salt, iterations, keylen, digest, callback);
};


exports.pbkdf2Sync = function(password, salt, iterations, keylen, digest) {
  return pbkdf2(password, salt, iterations, keylen, digest);
};


function pbkdf2(password, salt, iterations, keylen, digest, callback) {
  password = toBuf(password);
  salt = toBuf(salt);

  if (exports.DEFAULT_ENCODING === 'buffer')
    return binding.PBKDF2(password, salt, iterations, keylen, digest, callback);

  // at this point, we need to handle encodings.
  var encoding = exports.DEFAULT_ENCODING;
  if (callback) {
    function next(er, ret) {
      if (ret)
        ret = ret.toString(encoding);
      callback(er, ret);
    }
    binding.PBKDF2(password, salt, iterations, keylen, digest, next);
  } else {
    var ret = binding.PBKDF2(password, salt, iterations, keylen, digest);
    return ret.toString(encoding);
  }
}


exports.Certificate = Certificate;

function Certificate() {
  if (!(this instanceof Certificate))
    return new Certificate();

  this._handle = new binding.Certificate();
}


Certificate.prototype.verifySpkac = function(object) {
  return this._handle.verifySpkac(object);
};


Certificate.prototype.exportPublicKey = function(object, encoding) {
  return this._handle.exportPublicKey(toBuf(object, encoding));
};


Certificate.prototype.exportChallenge = function(object, encoding) {
  return this._handle.exportChallenge(toBuf(object, encoding));
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
    var key = name;
    if (/^[0-9A-Z\-]+$/.test(key)) key = key.toLowerCase();
    if (!ctx.hasOwnProperty(key) || ctx[key] < name)
      ctx[key] = name;
  });

  return Object.getOwnPropertyNames(ctx).map(function(key) {
    return ctx[key];
  }).sort();
}

// Legacy API
exports.__defineGetter__('createCredentials', util.deprecate(function() {
  return require('tls').createSecureContext;
}, 'createCredentials() is deprecated, use tls.createSecureContext instead'));

exports.__defineGetter__('Credentials', util.deprecate(function() {
  return require('tls').SecureContext;
}, 'Credentials is deprecated, use tls.createSecureContext instead'));
