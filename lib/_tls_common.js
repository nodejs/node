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
  ArrayPrototypeFilter,
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ObjectCreate,
  StringPrototypeReplace,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;

const { parseCertString } = require('internal/tls');
const { isArrayBufferView } = require('internal/util/types');
const tls = require('tls');
const {
  ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
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

const {
  validateString,
  validateInteger,
  validateInt32,
} = require('internal/validators');

const {
  toBuf
} = require('internal/crypto/util');

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

  if (secureOptions) {
    validateInteger(secureOptions, 'secureOptions');
    this.context.setOptions(secureOptions);
  }
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

function setKey(context, key, passphrase) {
  validateKeyOrCertOption('key', key);
  if (passphrase != null)
    validateString(passphrase, 'options.passphrase');
  context.setKey(key, passphrase);
}

function processCiphers(ciphers) {
  ciphers = StringPrototypeSplit(ciphers || tls.DEFAULT_CIPHERS, ':');

  const cipherList =
    ArrayPrototypeJoin(
      ArrayPrototypeFilter(
        ciphers,
        (cipher) => {
          return cipher.length > 0 &&
            !StringPrototypeStartsWith(cipher, 'TLS_');
        }), ':');

  const cipherSuites =
    ArrayPrototypeJoin(
      ArrayPrototypeFilter(
        ciphers,
        (cipher) => {
          return cipher.length > 0 &&
            StringPrototypeStartsWith(cipher, 'TLS_');
        }), ':');

  // Specifying empty cipher suites for both TLS1.2 and TLS1.3 is invalid, its
  // not possible to handshake with no suites.
  if (cipherSuites === '' && cipherList === '')
    throw new ERR_INVALID_ARG_VALUE('options.ciphers', ciphers);

  return { cipherList, cipherSuites };
}

function addCACerts(context, certs) {
  ArrayPrototypeForEach(certs, (cert) => {
    validateKeyOrCertOption('ca', cert);
    context.addCACert(cert);
  });
}

function setCerts(context, certs) {
  ArrayPrototypeForEach(certs, (cert) => {
    validateKeyOrCertOption('cert', cert);
    context.setCert(cert);
  });
}

exports.createSecureContext = function createSecureContext(options) {
  if (!options) options = {};

  const {
    ca,
    cert,
    ciphers,
    clientCertEngine,
    crl,
    dhparam,
    ecdhCurve = tls.DEFAULT_ECDH_CURVE,
    honorCipherOrder,
    key,
    minVersion,
    maxVersion,
    passphrase,
    pfx,
    privateKeyIdentifier,
    privateKeyEngine,
    secureProtocol,
    sessionIdContext,
    sessionTimeout,
    sigalgs,
    singleUse,
    ticketKeys,
  } = options;

  let { secureOptions } = options;

  if (honorCipherOrder)
    secureOptions |= SSL_OP_CIPHER_SERVER_PREFERENCE;

  const c = new SecureContext(secureProtocol, secureOptions,
                              minVersion, maxVersion);

  // Add CA before the cert to be able to load cert's issuer in C++ code.
  // NOTE(@jasnell): ca, cert, and key are permitted to be falsy, so do not
  // change the checks to !== undefined checks.
  if (ca) {
    if (ArrayIsArray(ca))
      addCACerts(c.context, ca);
    else
      addCACerts(c.context, [ca]);
  } else {
    c.context.addRootCerts();
  }

  if (cert) {
    if (ArrayIsArray(cert))
      setCerts(c.context, cert);
    else
      setCerts(c.context, [cert]);
  }

  // Set the key after the cert.
  // `ssl_set_pkey` returns `0` when the key does not match the cert, but
  // `ssl_set_cert` returns `1` and nullifies the key in the SSL structure
  // which leads to the crash later on.
  if (key) {
    if (ArrayIsArray(key)) {
      for (let i = 0; i < key.length; ++i) {
        const val = key[i];
        // eslint-disable-next-line eqeqeq
        const pem = (val != undefined && val.pem !== undefined ? val.pem : val);
        setKey(c.context, pem, val.passphrase || passphrase);
      }
    } else {
      setKey(c.context, key, passphrase);
    }
  }

  if (sigalgs !== undefined) {
    validateString(sigalgs, 'options.sigalgs');

    if (sigalgs === '')
      throw new ERR_INVALID_ARG_VALUE('options.sigalgs', sigalgs);

    c.context.setSigalgs(sigalgs);
  }

  if (privateKeyIdentifier !== undefined) {
    if (privateKeyEngine === undefined) {
      // Engine is required when privateKeyIdentifier is present
      throw new ERR_INVALID_ARG_VALUE('options.privateKeyEngine',
                                      privateKeyEngine);
    }
    if (key) {
      // Both data key and engine key can't be set at the same time
      throw new ERR_INVALID_ARG_VALUE('options.privateKeyIdentifier',
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

  if (ciphers != null)
    validateString(ciphers, 'options.ciphers');

  // Work around an OpenSSL API quirk. cipherList is for TLSv1.2 and below,
  // cipherSuites is for TLSv1.3 (and presumably any later versions). TLSv1.3
  // cipher suites all have a standard name format beginning with TLS_, so split
  // the ciphers and pass them to the appropriate API.
  const { cipherList, cipherSuites } = processCiphers(ciphers);

  c.context.setCipherSuites(cipherSuites);
  c.context.setCiphers(cipherList);

  if (cipherSuites === '' &&
      c.context.getMaxProto() > TLS1_2_VERSION &&
      c.context.getMinProto() < TLS1_3_VERSION) {
    c.context.setMaxProto(TLS1_2_VERSION);
  }

  if (cipherList === '' &&
      c.context.getMinProto() < TLS1_3_VERSION &&
      c.context.getMaxProto() > TLS1_2_VERSION) {
    c.context.setMinProto(TLS1_3_VERSION);
  }

  validateString(ecdhCurve, 'options.ecdhCurve');
  c.context.setECDHCurve(ecdhCurve);

  if (dhparam !== undefined) {
    validateKeyOrCertOption('dhparam', dhparam);
    const warning = c.context.setDHParam(dhparam);
    if (warning)
      process.emitWarning(warning, 'SecurityWarning');
  }

  if (crl !== undefined) {
    if (ArrayIsArray(crl)) {
      for (const val of crl) {
        validateKeyOrCertOption('crl', val);
        c.context.addCRL(val);
      }
    } else {
      validateKeyOrCertOption('crl', crl);
      c.context.addCRL(crl);
    }
  }

  if (sessionIdContext !== undefined) {
    validateString(sessionIdContext, 'options.sessionIdContext');
    c.context.setSessionIdContext(sessionIdContext);
  }

  if (pfx !== undefined) {
    if (ArrayIsArray(pfx)) {
      ArrayPrototypeForEach(pfx, (val) => {
        const raw = val.buf ? val.buf : val;
        const pass = val.passphrase || passphrase;
        if (pass !== undefined) {
          c.context.loadPKCS12(toBuf(raw), toBuf(pass));
        } else {
          c.context.loadPKCS12(toBuf(raw));
        }
      });
    } else if (passphrase) {
      c.context.loadPKCS12(toBuf(pfx), toBuf(passphrase));
    } else {
      c.context.loadPKCS12(toBuf(pfx));
    }
  }

  // Do not keep read/write buffers in free list for OpenSSL < 1.1.0. (For
  // OpenSSL 1.1.0, buffers are malloced and freed without the use of a
  // freelist.)
  if (singleUse) {
    c.singleUse = true;
    c.context.setFreeListLength(0);
  }

  if (clientCertEngine !== undefined) {
    if (typeof c.context.setClientCertEngine !== 'function')
      throw new ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED();
    if (typeof clientCertEngine !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('options.clientCertEngine',
                                     ['string', 'null', 'undefined'],
                                     clientCertEngine);
    }
    c.context.setClientCertEngine(clientCertEngine);
  }

  if (ticketKeys !== undefined) {
    if (!isArrayBufferView(ticketKeys)) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.ticketKeys',
        ['Buffer', 'TypedArray', 'DataView'],
        ticketKeys);
    }
    if (ticketKeys.byteLength !== 48) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.ticketKeys',
        ticketKeys.byteLenth,
        'must be exactly 48 bytes');
    }
    c.context.setTicketKeys(ticketKeys);
  }

  if (sessionTimeout !== undefined) {
    validateInt32(sessionTimeout, 'options.sessionTimeout');
    c.context.setSessionTimeout(sessionTimeout);
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
    StringPrototypeReplace(info, /([^\n:]*):([^\n]*)(?:\n|$)/g,
                           (all, key, val) => {
                             if (key in c.infoAccess)
                               ArrayPrototypePush(c.infoAccess[key], val);
                             else
                               c.infoAccess[key] = [val];
                           });
  }
  return c;
};
