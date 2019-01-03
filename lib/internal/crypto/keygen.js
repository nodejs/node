'use strict';

const { AsyncWrap, Providers } = internalBinding('async_wrap');
const {
  generateKeyPairRSA,
  generateKeyPairDSA,
  generateKeyPairEC,
  OPENSSL_EC_NAMED_CURVE,
  OPENSSL_EC_EXPLICIT_CURVE
} = internalBinding('crypto');
const {
  parsePublicKeyEncoding,
  parsePrivateKeyEncoding,

  PublicKeyObject,
  PrivateKeyObject
} = require('internal/crypto/keys');
const { customPromisifyArgs } = require('internal/util');
const { isUint32, validateString } = require('internal/validators');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_CALLBACK,
  ERR_INVALID_OPT_VALUE
} = require('internal/errors').codes;

const { isArrayBufferView } = require('internal/util/types');

function wrapKey(key, ctor) {
  if (typeof key === 'string' || isArrayBufferView(key))
    return key;
  return new ctor(key);
}

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
    // If no encoding was chosen, return key objects instead.
    pubkey = wrapKey(pubkey, PublicKeyObject);
    privkey = wrapKey(privkey, PrivateKeyObject);
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

  // If no encoding was chosen, return key objects instead.
  return {
    publicKey: wrapKey(publicKey, PublicKeyObject),
    privateKey: wrapKey(privateKey, PrivateKeyObject)
  };
}

function parseKeyEncoding(keyType, options) {
  const { publicKeyEncoding, privateKeyEncoding } = options;

  let publicFormat, publicType;
  if (publicKeyEncoding == null) {
    publicFormat = publicType = undefined;
  } else if (typeof publicKeyEncoding === 'object') {
    ({
      format: publicFormat,
      type: publicType
    } = parsePublicKeyEncoding(publicKeyEncoding, keyType,
                               'publicKeyEncoding'));
  } else {
    throw new ERR_INVALID_OPT_VALUE('publicKeyEncoding', publicKeyEncoding);
  }

  let privateFormat, privateType, cipher, passphrase;
  if (privateKeyEncoding == null) {
    privateFormat = privateType = undefined;
  } else if (typeof privateKeyEncoding === 'object') {
    ({
      format: privateFormat,
      type: privateType,
      cipher,
      passphrase
    } = parsePrivateKeyEncoding(privateKeyEncoding, keyType,
                                'privateKeyEncoding'));
  } else {
    throw new ERR_INVALID_OPT_VALUE('privateKeyEncoding', privateKeyEncoding);
  }

  return {
    cipher, passphrase, publicType, publicFormat, privateType, privateFormat
  };
}

function check(type, options, callback) {
  validateString(type, 'type');
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
                                            publicFormat, publicType,
                                            privateFormat, privateType,
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
                                            publicFormat, publicType,
                                            privateFormat, privateType,
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
                                           publicFormat, publicType,
                                           privateFormat, privateType,
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
