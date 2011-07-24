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


function Credentials(secureProtocol, context) {
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
}

exports.Credentials = Credentials;


exports.createCredentials = function(options, context) {
  if (!options) options = {};
  var c = new Credentials(options.secureProtocol, context);

  if (context) return c;

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

  if (options.crl) {
    if (Array.isArray(options.crl)) {
      for(var i = 0, len = options.crl.length; i < len; i++) {
        c.context.addCRL(options.crl[i]);
      }
    } else {
      c.context.addCRL(options.crl);
    }
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
exports.createCipher = function(cipher, password) {
  return (new Cipher).init(cipher, password);
};


exports.createCipheriv = function(cipher, key, iv) {
  return (new Cipher).initiv(cipher, key, iv);
};


exports.Decipher = Decipher;
exports.createDecipher = function(cipher, password) {
  return (new Decipher).init(cipher, password);
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
