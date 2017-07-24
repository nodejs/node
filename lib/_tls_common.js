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

'use strict';

const tls = require('tls');
const errors = require('internal/errors');

const SSL_OP_CIPHER_SERVER_PREFERENCE =
    process.binding('constants').crypto.SSL_OP_CIPHER_SERVER_PREFERENCE;

// Lazily loaded
var crypto = null;

const binding = process.binding('crypto');
const NativeSecureContext = binding.SecureContext;

function SecureContext(secureProtocol, secureOptions, context) {
  if (!(this instanceof SecureContext)) {
    return new SecureContext(secureProtocol, secureOptions, context);
  }

  if (context) {
    this.context = context;
  } else {
    this.context = new NativeSecureContext();

    if (secureProtocol) {
      this.context.init(secureProtocol);
    } else {
      this.context.init();
    }
  }

  if (secureOptions) this.context.setOptions(secureOptions);
}

function validateKeyCert(value, type) {
  if (typeof value !== 'string' && !ArrayBuffer.isView(value))
    throw new errors.TypeError(
      'ERR_INVALID_ARG_TYPE', type,
      ['string', 'Buffer', 'TypedArray', 'DataView']
    );
}

exports.SecureContext = SecureContext;


exports.createSecureContext = function createSecureContext(options, context) {
  if (!options) options = {};

  var secureOptions = options.secureOptions;
  if (options.honorCipherOrder)
    secureOptions |= SSL_OP_CIPHER_SERVER_PREFERENCE;

  var c = new SecureContext(options.secureProtocol, secureOptions, context);
  var i;
  var val;

  if (context) return c;

  // NOTE: It's important to add CA before the cert to be able to load
  // cert's issuer in C++ code.
  var ca = options.ca;
  if (ca !== undefined) {
    if (Array.isArray(ca)) {
      for (i = 0; i < ca.length; ++i) {
        val = ca[i];
        validateKeyCert(val, 'ca');
        c.context.addCACert(val);
      }
    } else {
      validateKeyCert(ca, 'ca');
      c.context.addCACert(ca);
    }
  } else {
    c.context.addRootCerts();
  }

  var cert = options.cert;
  if (cert !== undefined) {
    if (Array.isArray(cert)) {
      for (i = 0; i < cert.length; ++i) {
        val = cert[i];
        validateKeyCert(val, 'cert');
        c.context.setCert(val);
      }
    } else {
      validateKeyCert(cert, 'cert');
      c.context.setCert(cert);
    }
  }

  // NOTE: It is important to set the key after the cert.
  // `ssl_set_pkey` returns `0` when the key does not match the cert, but
  // `ssl_set_cert` returns `1` and nullifies the key in the SSL structure
  // which leads to the crash later on.
  var key = options.key;
  var passphrase = options.passphrase;
  if (key !== undefined) {
    if (Array.isArray(key)) {
      for (i = 0; i < key.length; ++i) {
        val = key[i];
        // eslint-disable-next-line eqeqeq
        const pem = (val != undefined && val.pem !== undefined ? val.pem : val);
        validateKeyCert(pem, 'key');
        c.context.setKey(pem, val.passphrase || passphrase);
      }
    } else {
      validateKeyCert(key, 'key');
      c.context.setKey(key, passphrase);
    }
  }

  if (options.ciphers)
    c.context.setCiphers(options.ciphers);
  else
    c.context.setCiphers(tls.DEFAULT_CIPHERS);

  if (options.ecdhCurve === undefined)
    c.context.setECDHCurve(tls.DEFAULT_ECDH_CURVE);
  else if (options.ecdhCurve)
    c.context.setECDHCurve(options.ecdhCurve);

  if (options.dhparam) {
    const warning = c.context.setDHParam(options.dhparam);
    if (warning)
      process.emitWarning(warning, 'SecurityWarning');
  }

  if (options.crl) {
    if (Array.isArray(options.crl)) {
      for (i = 0; i < options.crl.length; i++) {
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

    if (!crypto)
      crypto = require('crypto');

    pfx = crypto._toBuf(pfx);
    if (passphrase)
      passphrase = crypto._toBuf(passphrase);

    if (passphrase) {
      c.context.loadPKCS12(pfx, passphrase);
    } else {
      c.context.loadPKCS12(pfx);
    }
  }

  // Do not keep read/write buffers in free list for OpenSSL < 1.1.0. (For
  // OpenSSL 1.1.0, buffers are malloced and freed without the use of a
  // freelist.)
  if (options.singleUse) {
    c.singleUse = true;
    c.context.setFreeListLength(0);
  }

  return c;
};

exports.translatePeerCertificate = function translatePeerCertificate(c) {
  if (!c)
    return null;

  if (c.issuer != null) c.issuer = tls.parseCertString(c.issuer);
  if (c.issuerCertificate != null && c.issuerCertificate !== c) {
    c.issuerCertificate = translatePeerCertificate(c.issuerCertificate);
  }
  if (c.subject != null) c.subject = tls.parseCertString(c.subject);
  if (c.infoAccess != null) {
    var info = c.infoAccess;
    c.infoAccess = Object.create(null);

    // XXX: More key validation?
    info.replace(/([^\n:]*):([^\n]*)(?:\n|$)/g, function(all, key, val) {
      if (key in c.infoAccess)
        c.infoAccess[key].push(val);
      else
        c.infoAccess[key] = [val];
    });
  }
  return c;
};
