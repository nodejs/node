'use strict';

const {
  ObjectDefineProperty,
} = primordials;

const { AsyncWrap, Providers } = internalBinding('async_wrap');
const {
  generateKeyPairRSA,
  generateKeyPairRSAPSS,
  generateKeyPairDSA,
  generateKeyPairEC,
  generateKeyPairNid,
  generateKeyPairDH,
  EVP_PKEY_ED25519,
  EVP_PKEY_ED448,
  EVP_PKEY_X25519,
  EVP_PKEY_X448,
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
  ERR_INCOMPATIBLE_OPTION_PAIR,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_CALLBACK,
  ERR_INVALID_OPT_VALUE,
  ERR_MISSING_OPTION
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
    throw new ERR_INVALID_CALLBACK(callback);

  const wrap = new AsyncWrap(Providers.KEYPAIRGENREQUEST);
  wrap.ondone = (ex, pubkey, privkey) => {
    if (ex) return callback.call(wrap, ex);
    // If no encoding was chosen, return key objects instead.
    pubkey = wrapKey(pubkey, PublicKeyObject);
    privkey = wrapKey(privkey, PrivateKeyObject);
    callback.call(wrap, null, pubkey, privkey);
  };

  handleError(impl(wrap));
}

ObjectDefineProperty(generateKeyPair, customPromisifyArgs, {
  value: ['publicKey', 'privateKey'],
  enumerable: false
});

function generateKeyPairSync(type, options) {
  const impl = check(type, options);
  return handleError(impl());
}

function handleError(ret) {
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

  // These will be set after parsing the type and type-specific options to make
  // the order a bit more intuitive.
  let cipher, passphrase, publicType, publicFormat, privateType, privateFormat;

  if (options !== undefined && typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'object', options);

  function needOptions() {
    if (options == null)
      throw new ERR_INVALID_ARG_TYPE('options', 'object', options);
    return options;
  }

  let impl;
  switch (type) {
    case 'rsa':
    case 'rsa-pss':
      {
        const { modulusLength } = needOptions();
        if (!isUint32(modulusLength))
          throw new ERR_INVALID_OPT_VALUE('modulusLength', modulusLength);

        let { publicExponent } = options;
        if (publicExponent == null) {
          publicExponent = 0x10001;
        } else if (!isUint32(publicExponent)) {
          throw new ERR_INVALID_OPT_VALUE('publicExponent', publicExponent);
        }

        if (type === 'rsa') {
          impl = (wrap) => generateKeyPairRSA(modulusLength, publicExponent,
                                              publicFormat, publicType,
                                              privateFormat, privateType,
                                              cipher, passphrase, wrap);
          break;
        }

        const { hash, mgf1Hash, saltLength } = options;
        if (hash !== undefined && typeof hash !== 'string')
          throw new ERR_INVALID_OPT_VALUE('hash', hash);
        if (mgf1Hash !== undefined && typeof mgf1Hash !== 'string')
          throw new ERR_INVALID_OPT_VALUE('mgf1Hash', mgf1Hash);
        if (saltLength !== undefined && !isUint32(saltLength))
          throw new ERR_INVALID_OPT_VALUE('saltLength', saltLength);

        impl = (wrap) => generateKeyPairRSAPSS(modulusLength, publicExponent,
                                               hash, mgf1Hash, saltLength,
                                               publicFormat, publicType,
                                               privateFormat, privateType,
                                               cipher, passphrase, wrap);
      }
      break;
    case 'dsa':
      {
        const { modulusLength } = needOptions();
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
        const { namedCurve } = needOptions();
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
    case 'ed25519':
    case 'ed448':
    case 'x25519':
    case 'x448':
      {
        let id;
        switch (type) {
          case 'ed25519':
            id = EVP_PKEY_ED25519;
            break;
          case 'ed448':
            id = EVP_PKEY_ED448;
            break;
          case 'x25519':
            id = EVP_PKEY_X25519;
            break;
          case 'x448':
            id = EVP_PKEY_X448;
            break;
        }
        impl = (wrap) => generateKeyPairNid(id,
                                            publicFormat, publicType,
                                            privateFormat, privateType,
                                            cipher, passphrase, wrap);
      }
      break;
    case 'dh':
      {
        const { group, primeLength, prime, generator } = needOptions();
        let args;
        if (group != null) {
          if (prime != null)
            throw new ERR_INCOMPATIBLE_OPTION_PAIR('group', 'prime');
          if (primeLength != null)
            throw new ERR_INCOMPATIBLE_OPTION_PAIR('group', 'primeLength');
          if (generator != null)
            throw new ERR_INCOMPATIBLE_OPTION_PAIR('group', 'generator');
          if (typeof group !== 'string')
            throw new ERR_INVALID_OPT_VALUE('group', group);
          args = [group];
        } else {
          if (prime != null) {
            if (primeLength != null)
              throw new ERR_INCOMPATIBLE_OPTION_PAIR('prime', 'primeLength');
            if (!isArrayBufferView(prime))
              throw new ERR_INVALID_OPT_VALUE('prime', prime);
          } else if (primeLength != null) {
            if (!isUint32(primeLength))
              throw new ERR_INVALID_OPT_VALUE('primeLength', primeLength);
          } else {
            throw new ERR_MISSING_OPTION(
              'At least one of the group, prime, or primeLength options');
          }

          if (generator != null) {
            if (!isUint32(generator))
              throw new ERR_INVALID_OPT_VALUE('generator', generator);
          }

          args = [prime != null ? prime : primeLength,
                  generator == null ? 2 : generator];
        }

        impl = (wrap) => generateKeyPairDH(...args,
                                           publicFormat, publicType,
                                           privateFormat, privateType,
                                           cipher, passphrase, wrap);
      }
      break;
    default:
      throw new ERR_INVALID_ARG_VALUE('type', type,
                                      'must be a supported key type');
  }

  if (options) {
    ({
      cipher,
      passphrase,
      publicType,
      publicFormat,
      privateType,
      privateFormat
    } = parseKeyEncoding(type, options));
  }

  return impl;
}

module.exports = { generateKeyPair, generateKeyPairSync };
