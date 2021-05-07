'use strict';

const {
  FunctionPrototypeCall,
  ObjectDefineProperty,
  SafeArrayIterator,
} = primordials;

const {
  DhKeyPairGenJob,
  DsaKeyPairGenJob,
  EcKeyPairGenJob,
  NidKeyPairGenJob,
  RsaKeyPairGenJob,
  SecretKeyGenJob,
  kCryptoJobAsync,
  kCryptoJobSync,
  kKeyVariantRSA_PSS,
  kKeyVariantRSA_SSA_PKCS1_v1_5,
  EVP_PKEY_ED25519,
  EVP_PKEY_ED448,
  EVP_PKEY_X25519,
  EVP_PKEY_X448,
  OPENSSL_EC_NAMED_CURVE,
  OPENSSL_EC_EXPLICIT_CURVE,
} = internalBinding('crypto');

const {
  PublicKeyObject,
  PrivateKeyObject,
  SecretKeyObject,
  parsePublicKeyEncoding,
  parsePrivateKeyEncoding,
} = require('internal/crypto/keys');

const {
  kAesKeyLengths,
} = require('internal/crypto/util');

const { customPromisifyArgs } = require('internal/util');

const {
  isInt32,
  isUint32,
  validateCallback,
  validateString,
  validateInteger,
  validateObject,
  validateOneOf,
} = require('internal/validators');

const {
  codes: {
    ERR_INCOMPATIBLE_OPTION_PAIR,
    ERR_INVALID_ARG_VALUE,
    ERR_MISSING_OPTION,
  }
} = require('internal/errors');

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
  validateCallback(callback);

  const job = createJob(kCryptoJobAsync, type, options);

  job.ondone = (error, result) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    // If no encoding was chosen, return key objects instead.
    let { 0: pubkey, 1: privkey } = result;
    pubkey = wrapKey(pubkey, PublicKeyObject);
    privkey = wrapKey(privkey, PrivateKeyObject);
    FunctionPrototypeCall(callback, job, null, pubkey, privkey);
  };

  job.run();
}

ObjectDefineProperty(generateKeyPair, customPromisifyArgs, {
  value: ['publicKey', 'privateKey'],
  enumerable: false
});

function generateKeyPairSync(type, options) {
  return handleError(createJob(kCryptoJobSync, type, options).run());
}

function handleError(ret) {
  if (ret == null)
    return; // async

  const { 0: err, 1: keys } = ret;
  if (err !== undefined)
    throw err;

  const { 0: publicKey, 1: privateKey } = keys;

  // If no encoding was chosen, return key objects instead.
  return {
    publicKey: wrapKey(publicKey, PublicKeyObject),
    privateKey: wrapKey(privateKey, PrivateKeyObject)
  };
}

function parseKeyEncoding(keyType, options = {}) {
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
    throw new ERR_INVALID_ARG_VALUE('options.publicKeyEncoding',
                                    publicKeyEncoding);
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
    throw new ERR_INVALID_ARG_VALUE('options.privateKeyEncoding',
                                    privateKeyEncoding);
  }

  return [
    publicFormat,
    publicType,
    privateFormat,
    privateType,
    cipher,
    passphrase,
  ];
}

function createJob(mode, type, options) {
  validateString(type, 'type');

  const encoding = new SafeArrayIterator(parseKeyEncoding(type, options));

  if (options !== undefined)
    validateObject(options, 'options');

  switch (type) {
    case 'rsa':
    case 'rsa-pss':
    {
      validateObject(options, 'options');
      const { modulusLength } = options;
      if (!isUint32(modulusLength))
        throw new ERR_INVALID_ARG_VALUE('options.modulusLength', modulusLength);

      let { publicExponent } = options;
      if (publicExponent == null) {
        publicExponent = 0x10001;
      } else if (!isUint32(publicExponent)) {
        throw new ERR_INVALID_ARG_VALUE(
          'options.publicExponent',
          publicExponent);
      }

      if (type === 'rsa') {
        return new RsaKeyPairGenJob(
          mode,
          kKeyVariantRSA_SSA_PKCS1_v1_5,  // Used also for RSA-OAEP
          modulusLength,
          publicExponent,
          ...encoding);
      }

      const { hash, mgf1Hash, saltLength } = options;
      if (hash !== undefined && typeof hash !== 'string')
        throw new ERR_INVALID_ARG_VALUE('options.hash', hash);
      if (mgf1Hash !== undefined && typeof mgf1Hash !== 'string')
        throw new ERR_INVALID_ARG_VALUE('options.mgf1Hash', mgf1Hash);
      if (saltLength !== undefined && (!isInt32(saltLength) || saltLength < 0))
        throw new ERR_INVALID_ARG_VALUE('options.saltLength', saltLength);

      return new RsaKeyPairGenJob(
        mode,
        kKeyVariantRSA_PSS,
        modulusLength,
        publicExponent,
        hash,
        mgf1Hash,
        saltLength,
        ...encoding);
    }
    case 'dsa':
    {
      validateObject(options, 'options');
      const { modulusLength } = options;
      if (!isUint32(modulusLength))
        throw new ERR_INVALID_ARG_VALUE('options.modulusLength', modulusLength);

      let { divisorLength } = options;
      if (divisorLength == null) {
        divisorLength = -1;
      } else if (!isInt32(divisorLength) || divisorLength < 0) {
        throw new ERR_INVALID_ARG_VALUE('options.divisorLength', divisorLength);
      }

      return new DsaKeyPairGenJob(
        mode,
        modulusLength,
        divisorLength,
        ...encoding);
    }
    case 'ec':
    {
      validateObject(options, 'options');
      const { namedCurve } = options;
      if (typeof namedCurve !== 'string')
        throw new ERR_INVALID_ARG_VALUE('options.namedCurve', namedCurve);
      let { paramEncoding } = options;
      if (paramEncoding == null || paramEncoding === 'named')
        paramEncoding = OPENSSL_EC_NAMED_CURVE;
      else if (paramEncoding === 'explicit')
        paramEncoding = OPENSSL_EC_EXPLICIT_CURVE;
      else
        throw new ERR_INVALID_ARG_VALUE('options.paramEncoding', paramEncoding);

      return new EcKeyPairGenJob(
        mode,
        namedCurve,
        paramEncoding,
        ...encoding);
    }
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
      return new NidKeyPairGenJob(mode, id, ...encoding);
    }
    case 'dh':
    {
      validateObject(options, 'options');
      const { group, primeLength, prime, generator } = options;
      if (group != null) {
        if (prime != null)
          throw new ERR_INCOMPATIBLE_OPTION_PAIR('group', 'prime');
        if (primeLength != null)
          throw new ERR_INCOMPATIBLE_OPTION_PAIR('group', 'primeLength');
        if (generator != null)
          throw new ERR_INCOMPATIBLE_OPTION_PAIR('group', 'generator');
        if (typeof group !== 'string')
          throw new ERR_INVALID_ARG_VALUE('options.group', group);

        return new DhKeyPairGenJob(mode, group, ...encoding);
      }

      if (prime != null) {
        if (primeLength != null)
          throw new ERR_INCOMPATIBLE_OPTION_PAIR('prime', 'primeLength');
        if (!isArrayBufferView(prime))
          throw new ERR_INVALID_ARG_VALUE('options.prime', prime);
      } else if (primeLength != null) {
        if (!isInt32(primeLength) || primeLength < 0)
          throw new ERR_INVALID_ARG_VALUE('options.primeLength', primeLength);
      } else {
        throw new ERR_MISSING_OPTION(
          'At least one of the group, prime, or primeLength options');
      }

      if (generator != null) {
        if (!isInt32(generator) || generator < 0)
          throw new ERR_INVALID_ARG_VALUE('options.generator', generator);
      }
      return new DhKeyPairGenJob(
        mode,
        prime != null ? prime : primeLength,
        generator == null ? 2 : generator,
        ...encoding);
    }
    default:
      // Fall through
  }
  throw new ERR_INVALID_ARG_VALUE('type', type, 'must be a supported key type');
}

// Symmetric Key Generation

function generateKeyJob(mode, keyType, options) {
  validateString(keyType, 'type');
  validateObject(options, 'options');
  const { length } = options;
  switch (keyType) {
    case 'hmac':
      validateInteger(length, 'options.length', 1, 2 ** 31 - 1);
      break;
    case 'aes':
      validateOneOf(length, 'options.length', kAesKeyLengths);
      break;
    default:
      throw new ERR_INVALID_ARG_VALUE(
        'type',
        keyType,
        'must be a supported key type');
  }

  return new SecretKeyGenJob(mode, length);
}

function handleGenerateKeyError(ret) {
  if (ret === undefined)
    return; // async

  const { 0: err, 1: key } = ret;
  if (err !== undefined)
    throw err;

  return wrapKey(key, SecretKeyObject);
}

function generateKey(type, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = undefined;
  }

  validateCallback(callback);

  const job = generateKeyJob(kCryptoJobAsync, type, options);

  job.ondone = (error, key) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    FunctionPrototypeCall(callback, job, null, wrapKey(key, SecretKeyObject));
  };

  handleGenerateKeyError(job.run());
}

function generateKeySync(type, options) {
  return handleGenerateKeyError(
    generateKeyJob(kCryptoJobSync, type, options).run());
}

module.exports = {
  generateKeyPair,
  generateKeyPairSync,
  generateKey,
  generateKeySync,
};
