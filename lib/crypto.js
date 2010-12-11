
try {
  var binding = process.binding('crypto');
  var SecureContext = binding.SecureContext;
  var Hmac = binding.Hmac;
  var Hash = binding.Hash;
  var Cipher = binding.Cipher;
  var Decipher = binding.Decipher;
  var Sign = binding.Sign;
  var Verify = binding.Verify;
  var crypto = true;
} catch (e) {

  var crypto = false;
}


function Credentials(method) {
  if (!(this instanceof Credentials)) {
    return new Credentials(method);
  }

  if (!crypto) {
    throw new Error('node.js not compiled with openssl crypto support.');
  }

  this.context = new SecureContext();

  if (method) {
    this.context.init(method);
  } else {
    this.context.init();
  }

}

exports.Credentials = Credentials;


exports.createCredentials = function(options) {
  if (!options) options = {};
  var c = new Credentials(options.method);

  if (options.key) c.context.setKey(options.key);

  if (options.cert) c.context.setCert(options.cert);

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

  return c;
};


exports.Hash = Hash;
exports.createHash = function(hash) {
  return new Hash(hash);
};


exports.Hmac = Hmac;
exports.createHmac = function(hmac, key) {
  return (new Hmac).init(hmac, key);
};


exports.Cipher = Cipher;
exports.createCipher = function(cipher, key) {
  return (new Cipher).init(cipher, key);
};


exports.createCipheriv = function(cipher, key, iv) {
  return (new Cipher).initiv(cipher, key, iv);
};


exports.Decipher = Decipher;
exports.createDecipher = function(cipher, key) {
  return (new Decipher).init(cipher, key);
};


exports.createDecipheriv = function(cipher, key, iv) {
  return (new Decipher).initiv(cipher, key, iv);
};


exports.Sign = Sign;
exports.createSign = function(algorithm) {
  return (new Sign).init(algorithm);
};

exports.Verify = Verify;
exports.createVerify = function(algorithm) {
  return (new Verify).init(algorithm);
};
