'use strict';

const { AsyncWrap, Providers } = internalBinding('async_wrap');
const {
  generateKeyPairRSA,
  generateKeyPairDSA,
  generateKeyPairEC,
  OPENSSL_EC_NAMED_CURVE,
  OPENSSL_EC_EXPLICIT_CURVE,
  PK_ENCODING_PKCS1,
  PK_ENCODING_PKCS8,
  PK_ENCODING_SPKI,
  PK_ENCODING_SEC1,
  PK_FORMAT_DER,
  PK_FORMAT_PEM
} = process.binding('crypto');
const { customPromisifyArgs } = require('internal/util');
const { isUint32 } = require('internal/validators');
const {
  ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_CALLBACK,
  ERR_INVALID_OPT_VALUE
} = require('internal/errors').codes;

function generateKeyPair(type, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = undefined;
  }

  const impl = check(type, options);

  if (typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK();

  const wrap = new AsyncWrap(Providers.KEYPAIRGENREQUEST);
  wrap.ondone = (ex, pubkey, privkey) => {
    if (ex) return callback.call(wrap, ex);
    callback.call(wrap, null, pubkey, privkey);
  };

  handleError(impl, wrap);
}

Object.defineProperty(generateKeyPair, customPromisifyArgs, {
  value: ['publicKey', 'privateKey'],
  enumerable: false
});

function generateKeyPairSync(type, options) {
  const impl = check(type, options);
  return handleError(impl);
}

function handleError(impl, wrap) {
  const ret = impl(wrap);
  if (ret === undefined)
    return; // async

  const [err, publicKey, privateKey] = ret;
  if (err !== undefined)
    throw err;

  return { publicKey, privateKey };
}

function parseKeyEncoding(keyType, options) {
  const { publicKeyEncoding, privateKeyEncoding } = options;

  if (publicKeyEncoding == null || typeof publicKeyEncoding !== 'object')
    throw new ERR_INVALID_OPT_VALUE('publicKeyEncoding', publicKeyEncoding);

  const { format: strPublicFormat, type: strPublicType } = publicKeyEncoding;

  let publicType;
  if (strPublicType === 'pkcs1') {
    if (keyType !== 'rsa') {
      throw new ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(
        strPublicType, 'can only be used for RSA keys');
    }
    publicType = PK_ENCODING_PKCS1;
  } else if (strPublicType === 'spki') {
    publicType = PK_ENCODING_SPKI;
  } else {
    throw new ERR_INVALID_OPT_VALUE('publicKeyEncoding.type', strPublicType);
  }

  let publicFormat;
  if (strPublicFormat === 'der') {
    publicFormat = PK_FORMAT_DER;
  } else if (strPublicFormat === 'pem') {
    publicFormat = PK_FORMAT_PEM;
  } else {
    throw new ERR_INVALID_OPT_VALUE('publicKeyEncoding.format',
                                    strPublicFormat);
  }

  if (privateKeyEncoding == null || typeof privateKeyEncoding !== 'object')
    throw new ERR_INVALID_OPT_VALUE('privateKeyEncoding', privateKeyEncoding);

  const {
    cipher,
    passphrase,
    format: strPrivateFormat,
    type: strPrivateType
  } = privateKeyEncoding;

  let privateType;
  if (strPrivateType === 'pkcs1') {
    if (keyType !== 'rsa') {
      throw new ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(
        strPrivateType, 'can only be used for RSA keys');
    }
    privateType = PK_ENCODING_PKCS1;
  } else if (strPrivateType === 'pkcs8') {
    privateType = PK_ENCODING_PKCS8;
  } else if (strPrivateType === 'sec1') {
    if (keyType !== 'ec') {
      throw new ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(
        strPrivateType, 'can only be used for EC keys');
    }
    privateType = PK_ENCODING_SEC1;
  } else {
    throw new ERR_INVALID_OPT_VALUE('privateKeyEncoding.type', strPrivateType);
  }

  let privateFormat;
  if (strPrivateFormat === 'der') {
    privateFormat = PK_FORMAT_DER;
  } else if (strPrivateFormat === 'pem') {
    privateFormat = PK_FORMAT_PEM;
  } else {
    throw new ERR_INVALID_OPT_VALUE('privateKeyEncoding.format',
                                    strPrivateFormat);
  }

  if (cipher != null) {
    if (typeof cipher !== 'string')
      throw new ERR_INVALID_OPT_VALUE('privateKeyEncoding.cipher', cipher);
    if (privateFormat === PK_FORMAT_DER &&
        (privateType === PK_ENCODING_PKCS1 ||
         privateType === PK_ENCODING_SEC1)) {
      throw new ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(
        strPrivateType, 'does not support encryption');
    }
    if (typeof passphrase !== 'string') {
      throw new ERR_INVALID_OPT_VALUE('privateKeyEncoding.passphrase',
                                      passphrase);
    }
  }

  return {
    cipher, passphrase, publicType, publicFormat, privateType, privateFormat
  };
}

function check(type, options, callback) {
  if (typeof type !== 'string')
    throw new ERR_INVALID_ARG_TYPE('type', 'string', type);
  if (options == null || typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'object', options);

  // These will be set after parsing the type and type-specific options to make
  // the order a bit more intuitive.
  let cipher, passphrase, publicType, publicFormat, privateType, privateFormat;

  let impl;
  switch (type) {
    case 'rsa':
      {
        const { modulusLength } = options;
        if (!isUint32(modulusLength))
          throw new ERR_INVALID_OPT_VALUE('modulusLength', modulusLength);

        let { publicExponent } = options;
        if (publicExponent == null) {
          publicExponent = 0x10001;
        } else if (!isUint32(publicExponent)) {
          throw new ERR_INVALID_OPT_VALUE('publicExponent', publicExponent);
        }

        impl = (wrap) => generateKeyPairRSA(modulusLength, publicExponent,
                                            publicType, publicFormat,
                                            privateType, privateFormat,
                                            cipher, passphrase, wrap);
      }
      break;
    case 'dsa':
      {
        const { modulusLength } = options;
        if (!isUint32(modulusLength))
          throw new ERR_INVALID_OPT_VALUE('modulusLength', modulusLength);

        let { divisorLength } = options;
        if (divisorLength == null) {
          divisorLength = -1;
        } else if (!isUint32(divisorLength)) {
          throw new ERR_INVALID_OPT_VALUE('divisorLength', divisorLength);
        }

        impl = (wrap) => generateKeyPairDSA(modulusLength, divisorLength,
                                            publicType, publicFormat,
                                            privateType, privateFormat,
                                            cipher, passphrase, wrap);
      }
      break;
    case 'ec':
      {
        const { namedCurve } = options;
        if (typeof namedCurve !== 'string')
          throw new ERR_INVALID_OPT_VALUE('namedCurve', namedCurve);
        let { paramEncoding } = options;
        if (paramEncoding == null || paramEncoding === 'named')
          paramEncoding = OPENSSL_EC_NAMED_CURVE;
        else if (paramEncoding === 'explicit')
          paramEncoding = OPENSSL_EC_EXPLICIT_CURVE;
        else
          throw new ERR_INVALID_OPT_VALUE('paramEncoding', paramEncoding);

        impl = (wrap) => generateKeyPairEC(namedCurve, paramEncoding,
                                           publicType, publicFormat,
                                           privateType, privateFormat,
                                           cipher, passphrase, wrap);
      }
      break;
    default:
      throw new ERR_INVALID_ARG_VALUE('type', type,
                                      "must be one of 'rsa', 'dsa', 'ec'");
  }

  ({
    cipher,
    passphrase,
    publicType,
    publicFormat,
    privateType,
    privateFormat
  } = parseKeyEncoding(type, options));

  return impl;
}

module.exports = { generateKeyPair, generateKeyPairSync };
