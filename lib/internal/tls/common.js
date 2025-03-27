'use strict';

const {
  JSONParse,
} = primordials;

const tls = require('tls');

const {
  codes: {
    ERR_TLS_INVALID_PROTOCOL_VERSION,
    ERR_TLS_PROTOCOL_VERSION_CONFLICT,
  },
} = require('internal/errors');

const {
  crypto: {
    SSL_OP_CIPHER_SERVER_PREFERENCE,
    TLS1_VERSION,
    TLS1_1_VERSION,
    TLS1_2_VERSION,
    TLS1_3_VERSION,
  },
} = internalBinding('constants');

const {
  kEmptyObject,
} = require('internal/util');

const {
  validateInteger,
} = require('internal/validators');

const {
  configSecureContext,
} = require('internal/tls/secure-context');

function toV(which, v, def) {
  v ??= def;
  if (v === 'TLSv1') return TLS1_VERSION;
  if (v === 'TLSv1.1') return TLS1_1_VERSION;
  if (v === 'TLSv1.2') return TLS1_2_VERSION;
  if (v === 'TLSv1.3') return TLS1_3_VERSION;
  throw new ERR_TLS_INVALID_PROTOCOL_VERSION(v, which);
}

const {
  SecureContext: NativeSecureContext,
} = internalBinding('crypto');

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

function createSecureContext(options) {
  options ||= kEmptyObject;
  const {
    honorCipherOrder,
    minVersion,
    maxVersion,
    secureProtocol,
  } = options;

  let { secureOptions } = options;

  if (honorCipherOrder)
    secureOptions |= SSL_OP_CIPHER_SERVER_PREFERENCE;

  const c = new SecureContext(secureProtocol, secureOptions,
                              minVersion, maxVersion);

  configSecureContext(c.context, options);

  return c;
}

// Translate some fields from the handle's C-friendly format into more idiomatic
// javascript object representations before passing them back to the user.  Can
// be used on any cert object, but changing the name would be semver-major.
function translatePeerCertificate(c) {
  if (!c)
    return null;

  if (c.issuerCertificate != null && c.issuerCertificate !== c) {
    c.issuerCertificate = translatePeerCertificate(c.issuerCertificate);
  }
  if (c.infoAccess != null) {
    const info = c.infoAccess;
    c.infoAccess = { __proto__: null };

    // XXX: More key validation?
    info.replace(/([^\n:]*):([^\n]*)(?:\n|$)/g,
                 (all, key, val) => {
                   if (val.charCodeAt(0) === 0x22) {
                     // The translatePeerCertificate function is only
                     // used on internally created legacy certificate
                     // objects, and any value that contains a quote
                     // will always be a valid JSON string literal,
                     // so this should never throw.
                     val = JSONParse(val);
                   }
                   if (key in c.infoAccess)
                     c.infoAccess[key].push(val);
                   else
                     c.infoAccess[key] = [val];
                 });
  }
  return c;
}

module.exports = {
  SecureContext,
  createSecureContext,
  translatePeerCertificate,
};
