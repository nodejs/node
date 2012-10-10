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


try {
  var binding = process.binding('crypto');
  var SecureContext = binding.SecureContext;
  var PBKDF2 = binding.PBKDF2;
  var randomBytes = binding.randomBytes;
  var pseudoRandomBytes = binding.pseudoRandomBytes;
  var getCiphers = binding.getCiphers;
  var getHashes = binding.getHashes;
  var crypto = true;
} catch (e) {

  var crypto = false;
}

var assert = require('assert');
var StringDecoder = require('string_decoder').StringDecoder;

function Credentials(secureProtocol, flags, context) {
  if (!(this instanceof Credentials)) {
    return new Credentials(secureProtocol);
  }

  if (!crypto) {
    throw new Error('node.js not compiled with openssl crypto support.');
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

  if (options.ca) {
    if (Array.isArray(options.ca)) {
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
    if (Array.isArray(options.crl)) {
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
    if (options.passphrase) {
      c.context.loadPKCS12(options.pfx, options.passphrase);
    } else {
      c.context.loadPKCS12(options.pfx);
    }
  }

  return c;
};


exports.createHash = exports.Hash = Hash;
function Hash(algorithm) {
  if (!(this instanceof Hash))
    return new Hash(algorithm);
  this._binding = new binding.Hash(algorithm);
}

Hash.prototype.update = function(data, encoding) {
  if (encoding === 'buffer')
    encoding = null;
  if (encoding || typeof data === 'string')
    data = new Buffer(data, encoding);
  this._binding.update(data);
  return this;
};

Hash.prototype.digest = function(outputEncoding) {
  var result = this._binding.digest('buffer');
  if (outputEncoding && outputEncoding !== 'buffer')
    result = result.toString(outputEncoding);
  return result;
};


exports.createHmac = exports.Hmac = Hmac;

function Hmac(hmac, key) {
  if (!(this instanceof Hmac))
    return new Hmac(hmac, key);
  this._binding = new binding.Hmac();
  this._binding.init(hmac, key);
};

Hmac.prototype.update = Hash.prototype.update;
Hmac.prototype.digest = Hash.prototype.digest;


function getDecoder(decoder, encoding) {
  decoder = decoder || new StringDecoder(encoding);
  assert(decoder.encoding === encoding, 'Cannot change encoding');
  return decoder;
}


exports.createCipher = exports.Cipher = Cipher;
function Cipher(cipher, password) {
  if (!(this instanceof Cipher))
    return new Cipher(cipher, password);
  this._binding = new binding.Cipher;
  this._binding.init(cipher, password);
  this._decoder = null;
};

Cipher.prototype.update = function(data, inputEncoding, outputEncoding) {
  if (inputEncoding && inputEncoding !== 'buffer')
    data = new Buffer(data, inputEncoding);

  var ret = this._binding.update(data, 'buffer', 'buffer');

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    ret = this._decoder.write(ret);
  }

  return ret;
};

Cipher.prototype.final = function(outputEncoding) {
  var ret = this._binding.final('buffer');

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    ret = this._decoder.write(ret);
  }

  return ret;
};

Cipher.prototype.setAutoPadding = function(ap) {
  this._binding.setAutoPadding(ap);
  return this;
};



exports.createCipheriv = exports.Cipheriv = Cipheriv;
function Cipheriv(cipher, key, iv) {
  if (!(this instanceof Cipheriv))
    return new Cipheriv(cipher, key, iv);
  this._binding = new binding.Cipher();
  this._binding.initiv(cipher, key, iv);
  this._decoder = null;
}

Cipheriv.prototype.update = Cipher.prototype.update;
Cipheriv.prototype.final = Cipher.prototype.final;
Cipheriv.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;


exports.createDecipher = exports.Decipher = Decipher;
function Decipher(cipher, password) {
  if (!(this instanceof Decipher))
    return new Decipher(cipher, password);
  this._binding = new binding.Decipher
  this._binding.init(cipher, password);
  this._decoder = null;
};

Decipher.prototype.update = Cipher.prototype.update;
Decipher.prototype.final = Cipher.prototype.final;
Decipher.prototype.finaltol = Cipher.prototype.final;
Decipher.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;


exports.createDecipheriv = exports.Decipheriv = Decipheriv;
function Decipheriv(cipher, key, iv) {
  if (!(this instanceof Decipheriv))
    return new Decipheriv(cipher, key, iv);
  this._binding = new binding.Decipher;
  this._binding.initiv(cipher, key, iv);
  this._decoder = null;
};

Decipheriv.prototype.update = Cipher.prototype.update;
Decipheriv.prototype.final = Cipher.prototype.final;
Decipheriv.prototype.finaltol = Cipher.prototype.final;
Decipheriv.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;


exports.createSign = exports.Sign = Sign;
function Sign(algorithm) {
  if (!(this instanceof Sign))
    return new Sign(algorithm);
  this._binding = new binding.Sign();
  this._binding.init(algorithm);
};

Sign.prototype.update = Hash.prototype.update;

Sign.prototype.sign = function(key, encoding) {
  var ret = this._binding.sign(key, 'buffer');
  if (encoding && encoding !== 'buffer')
    ret = ret.toString(encoding);
  return ret;
};


exports.createVerify = exports.Verify = Verify;
function Verify(algorithm) {
  if (!(this instanceof Verify))
    return new Verify(algorithm);

  this._binding = new binding.Verify;
  this._binding.init(algorithm);
}

Verify.prototype.update = Hash.prototype.update;

Verify.prototype.verify = function(object, signature, sigEncoding) {
  if (sigEncoding === 'buffer')
    sigEncoding = null;
  if (sigEncoding || typeof signature === 'string')
    signature = new Buffer(signature, sigEncoding);
  return this._binding.verify(object, signature, 'buffer');
};

exports.createDiffieHellman = exports.DiffieHellman = DiffieHellman;

function DiffieHellman(sizeOrKey, encoding) {
  if (!(this instanceof DiffieHellman))
    return new DiffieHellman(sizeOrKey, encoding);

  if (!sizeOrKey)
    this._binding = new binding.DiffieHellman();
  else {
    if (encoding === 'buffer')
      encoding = null;
    if (encoding || typeof sizeOrKey === 'string')
      sizeOrKey = new Buffer(sizeOrKey, encoding);
    this._binding = new binding.DiffieHellman(sizeOrKey, 'buffer');
  }
}

DiffieHellman.prototype.generateKeys = function(encoding) {
  var keys = this._binding.generateKeys('buffer');
  if (encoding)
    keys = keys.toString(encoding);
  return keys;
};

DiffieHellman.prototype.computeSecret = function(key, inEnc, outEnc) {
  if (inEnc === 'buffer')
    inEnc = null;
  if (outEnc === 'buffer')
    outEnc = null;
  if (inEnc || typeof key === 'string')
    key = new Buffer(key, inEnc);
  var ret = this._binding.computeSecret(key, 'buffer', 'buffer');
  if (outEnc)
    ret = ret.toString(outEnc);
  return ret;
};

DiffieHellman.prototype.getPrime = function(encoding) {
  var prime = this._binding.getPrime('buffer');
  if (encoding && encoding !== 'buffer')
    prime = prime.toString(encoding);
  return prime;
};

DiffieHellman.prototype.getGenerator = function(encoding) {
  var generator = this._binding.getGenerator('buffer');
  if (encoding && encoding !== 'buffer')
    generator = generator.toString(encoding);
  return generator;
};

DiffieHellman.prototype.getPublicKey = function(encoding) {
  var key = this._binding.getPublicKey('buffer');
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
};

DiffieHellman.prototype.getPrivateKey = function(encoding) {
  var key = this._binding.getPrivateKey('buffer');
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
};

DiffieHellman.prototype.setPublicKey = function(key, encoding) {
  if (encoding === 'buffer')
    encoding = null;
  if (encoding || typeof key === 'string')
    key = new Buffer(key, encoding);
  this._binding.setPublicKey(key, 'buffer');
  return this;
};

DiffieHellman.prototype.setPrivateKey = function(key, encoding) {
  if (encoding === 'buffer')
    encoding = null;
  if (encoding || typeof key === 'string')
    key = new Buffer(key, encoding);
  this._binding.setPrivateKey(key, 'buffer');
  return this;
};



exports.DiffieHellmanGroup =
  exports.createDiffieHellmanGroup =
  exports.getDiffieHellman = DiffieHellmanGroup;

function DiffieHellmanGroup(name) {
  if (!(this instanceof DiffieHellmanGroup))
    return new DiffieHellmanGroup(name);
  this._binding = new binding.DiffieHellmanGroup(name);
};

DiffieHellmanGroup.prototype.generateKeys =
  DiffieHellman.prototype.generateKeys;

DiffieHellmanGroup.prototype.computeSecret =
  DiffieHellman.prototype.computeSecret;

DiffieHellmanGroup.prototype.getPrime =
  DiffieHellman.prototype.getPrime;

DiffieHellmanGroup.prototype.getGenerator =
  DiffieHellman.prototype.getGenerator;

DiffieHellmanGroup.prototype.getPublicKey =
  DiffieHellman.prototype.getPublicKey;

DiffieHellmanGroup.prototype.getPrivateKey =
  DiffieHellman.prototype.getPrivateKey;

exports.pbkdf2 = PBKDF2;

exports.randomBytes = randomBytes;
exports.pseudoRandomBytes = pseudoRandomBytes;

exports.rng = randomBytes;
exports.prng = pseudoRandomBytes;


exports.getCiphers = function() {
  return getCiphers.call(null, arguments).sort();
};


exports.getHashes = function() {
  var names = getHashes.call(null, arguments);

  // Drop all-caps names in favor of their lowercase aliases,
  // for example, 'sha1' instead of 'SHA1'.
  var ctx = {};
  names = names.forEach(function(name) {
    if (/^[0-9A-Z\-]+$/.test(name)) name = name.toLowerCase();
    ctx[name] = true;
  });
  names = Object.getOwnPropertyNames(ctx);

  return names.sort();
};
