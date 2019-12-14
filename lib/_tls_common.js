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

const {
  ArrayIsArray,
  ObjectCreate,
} = primordials;

const { parseCertString } = require('internal/tls');
const { isArrayBufferView } = require('internal/util/types');
const tls = require('tls');
const {
  ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_OPT_VALUE,
  ERR_TLS_INVALID_PROTOCOL_VERSION,
  ERR_TLS_PROTOCOL_VERSION_CONFLICT,
} = require('internal/errors').codes;
const {
  SSL_OP_CIPHER_SERVER_PREFERENCE,
  TLS1_VERSION,
  TLS1_1_VERSION,
  TLS1_2_VERSION,
  TLS1_3_VERSION,
} = internalBinding('constants').crypto;

// Lazily loaded from internal/crypto/util.
let toBuf = null;

function toV(which, v, def) {
  if (v == null) v = def;
  if (v === 'TLSv1') return TLS1_VERSION;
  if (v === 'TLSv1.1') return TLS1_1_VERSION;
  if (v === 'TLSv1.2') return TLS1_2_VERSION;
  if (v === 'TLSv1.3') return TLS1_3_VERSION;
  throw new ERR_TLS_INVALID_PROTOCOL_VERSION(v, which);
}

const { SecureContext: NativeSecureContext } = internalBinding('crypto');
function SecureContext(secureProtocol, secureOptions, minVersion, maxVersion) {
  if (!(this instanceof SecureContext)) {
    return new SecureContext(secureProtocol, secureOptions, minVersion,
                             maxVersion);
  }

  if (secureProtocol) {
    if (minVersion != null)
      throw new ERR_TLS_PROTOCOL_VERSION_CONFLICT(minVersion, secureProtocol);
    if (maxVersion != null)
      throw new ERR_TLS_PROTOCOL_VERSION_CONFLICT(maxVersion, secureProtocol);
  }

  this.context = new NativeSecureContext();
  this.context.init(secureProtocol,
                    toV('minimum', minVersion, tls.DEFAULT_MIN_VERSION),
                    toV('maximum', maxVersion, tls.DEFAULT_MAX_VERSION));

  if (secureOptions) this.context.setOptions(secureOptions);
}

function validateKeyOrCertOption(name, value) {
  if (typeof value !== 'string' && !isArrayBufferView(value)) {
    throw new ERR_INVALID_ARG_TYPE(
      `options.${name}`,
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      value
    );
  }
}

exports.SecureContext = SecureContext;


exports.createSecureContext = function createSecureContext(options) {
  if (!options) options = {};

  let secureOptions = options.secureOptions;
  if (options.honorCipherOrder)
    secureOptions |= SSL_OP_CIPHER_SERVER_PREFERENCE;

  const c = new SecureContext(options.secureProtocol, secureOptions,
                              options.minVersion, options.maxVersion);

  // Add CA before the cert to be able to load cert's issuer in C++ code.
  const { ca } = options;
  if (ca) {
    if (ArrayIsArray(ca)) {
      for (const val of ca) {
        validateKeyOrCertOption('ca', val);
        c.context.addCACert(val);
      }
    } else {
      validateKeyOrCertOption('ca', ca);
      c.context.addCACert(ca);
    }
  } else {
    c.context.addRootCerts();
  }

  const { cert } = options;
  if (cert) {
    if (ArrayIsArray(cert)) {
      for (const val of cert) {
        validateKeyOrCertOption('cert', val);
        c.context.setCert(val);
      }
    } else {
      validateKeyOrCertOption('cert', cert);
      c.context.setCert(cert);
    }
  }

  // Set the key after the cert.
  // `ssl_set_pkey` returns `0` when the key does not match the cert, but
  // `ssl_set_cert` returns `1` and nullifies the key in the SSL structure
  // which leads to the crash later on.
  const key = options.key;
  const passphrase = options.passphrase;
  if (key) {
    if (ArrayIsArray(key)) {
      for (const val of key) {
        // eslint-disable-next-line eqeqeq
        const pem = (val != undefined && val.pem !== undefined ? val.pem : val);
        validateKeyOrCertOption('key', pem);
        c.context.setKey(pem, val.passphrase || passphrase);
      }
    } else {
      validateKeyOrCertOption('key', key);
      c.context.setKey(key, passphrase);
    }
  }

  const sigalgs = options.sigalgs;
  if (sigalgs !== undefined) {
    if (typeof sigalgs !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('options.sigalgs', 'string', sigalgs);
    }

    if (sigalgs === '') {
      throw new ERR_INVALID_OPT_VALUE('sigalgs', sigalgs);
    }

    c.context.setSigalgs(sigalgs);
  }

  const { privateKeyIdentifier, privateKeyEngine } = options;
  if (privateKeyIdentifier !== undefined) {
    if (privateKeyEngine === undefined) {
      // Engine is required when privateKeyIdentifier is present
      throw new ERR_INVALID_OPT_VALUE('privateKeyEngine',
                                      privateKeyEngine);
    }
    if (key) {
      // Both data key and engine key can't be set at the same time
      throw new ERR_INVALID_OPT_VALUE('privateKeyIdentifier',
                                      privateKeyIdentifier);
    }

    if (typeof privateKeyIdentifier === 'string' &&
        typeof privateKeyEngine === 'string') {
      if (c.context.setEngineKey)
        c.context.setEngineKey(privateKeyIdentifier, privateKeyEngine);
      else
        throw new ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED();
    } else if (typeof privateKeyIdentifier !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('options.privateKeyIdentifier',
                                     ['string', 'undefined'],
                                     privateKeyIdentifier);
    } else {
      throw new ERR_INVALID_ARG_TYPE('options.privateKeyEngine',
                                     ['string', 'undefined'],
                                     privateKeyEngine);
    }
  }

  if (options.ciphers && typeof options.ciphers !== 'string') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.ciphers', 'string', options.ciphers);
  }

  // Work around an OpenSSL API quirk. cipherList is for TLSv1.2 and below,
  // cipherSuites is for TLSv1.3 (and presumably any later versions). TLSv1.3
  // cipher suites all have a standard name format beginning with TLS_, so split
  // the ciphers and pass them to the appropriate API.
  const ciphers = (options.ciphers || tls.DEFAULT_CIPHERS).split(':');
  const cipherList = ciphers.filter((_) => !_.match(/^TLS_/) &&
                                    _.length > 0).join(':');
  const cipherSuites = ciphers.filter((_) => _.match(/^TLS_/)).join(':');

  if (cipherSuites === '' && cipherList === '') {
    // Specifying empty cipher suites for both TLS1.2 and TLS1.3 is invalid, its
    // not possible to handshake with no suites.
    throw new ERR_INVALID_OPT_VALUE('ciphers', ciphers);
  }

  c.context.setCipherSuites(cipherSuites);
  c.context.setCiphers(cipherList);

  if (cipherSuites === '' && c.context.getMaxProto() > TLS1_2_VERSION &&
      c.context.getMinProto() < TLS1_3_VERSION)
    c.context.setMaxProto(TLS1_2_VERSION);

  if (cipherList === '' && c.context.getMinProto() < TLS1_3_VERSION &&
      c.context.getMaxProto() > TLS1_2_VERSION)
    c.context.setMinProto(TLS1_3_VERSION);

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
    if (ArrayIsArray(options.crl)) {
      for (const crl of options.crl) {
        c.context.addCRL(crl);
      }
    } else {
      c.context.addCRL(options.crl);
    }
  }

  if (options.sessionIdContext) {
    c.context.setSessionIdContext(options.sessionIdContext);
  }

  if (options.pfx) {
    if (!toBuf)
      toBuf = require('internal/crypto/util').toBuf;

    if (ArrayIsArray(options.pfx)) {
      for (const pfx of options.pfx) {
        const raw = pfx.buf ? pfx.buf : pfx;
        const buf = toBuf(raw);
        const passphrase = pfx.passphrase || options.passphrase;
        if (passphrase) {
          c.context.loadPKCS12(buf, toBuf(passphrase));
        } else {
          c.context.loadPKCS12(buf);
        }
      }
    } else {
      const buf = toBuf(options.pfx);
      const passphrase = options.passphrase;
      if (passphrase) {
        c.context.loadPKCS12(buf, toBuf(passphrase));
      } else {
        c.context.loadPKCS12(buf);
      }
    }
  }

  // Do not keep read/write buffers in free list for OpenSSL < 1.1.0. (For
  // OpenSSL 1.1.0, buffers are malloced and freed without the use of a
  // freelist.)
  if (options.singleUse) {
    c.singleUse = true;
    c.context.setFreeListLength(0);
  }

  if (typeof options.clientCertEngine === 'string') {
    if (c.context.setClientCertEngine)
      c.context.setClientCertEngine(options.clientCertEngine);
    else
      throw new ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED();
  } else if (options.clientCertEngine != null) {
    throw new ERR_INVALID_ARG_TYPE('options.clientCertEngine',
                                   ['string', 'null', 'undefined'],
                                   options.clientCertEngine);
  }

  return c;
};

// Translate some fields from the handle's C-friendly format into more idiomatic
// javascript object representations before passing them back to the user.  Can
// be used on any cert object, but changing the name would be semver-major.
exports.translatePeerCertificate = function translatePeerCertificate(c) {
  if (!c)
    return null;

  if (c.issuer != null) c.issuer = parseCertString(c.issuer);
  if (c.issuerCertificate != null && c.issuerCertificate !== c) {
    c.issuerCertificate = translatePeerCertificate(c.issuerCertificate);
  }
  if (c.subject != null) c.subject = parseCertString(c.subject);
  if (c.infoAccess != null) {
    const info = c.infoAccess;
    c.infoAccess = ObjectCreate(null);

    // XXX: More key validation?
    info.replace(/([^\n:]*):([^\n]*)(?:\n|$)/g, (all, key, val) => {
      if (key in c.infoAccess)
        c.infoAccess[key].push(val);
      else
        c.infoAccess[key] = [val];
    });
  }
  return c;
};
