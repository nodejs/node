'use strict';

const {
  MathCeil,
  SafeSet,
  TypedArrayPrototypeGetBuffer,
  Uint8Array,
} = primordials;

const {
  RSACipherJob,
  SignJob,
  kCryptoJobWebCrypto,
  kKeyFormatDER,
  kKeyTypePublic,
  kSignJobModeSign,
  kSignJobModeVerify,
  kKeyVariantRSA_OAEP,
  kKeyVariantRSA_SSA_PKCS1_v1_5,
  kWebCryptoCipherEncrypt,
  kWebCryptoKeyFormatPKCS8,
  kWebCryptoKeyFormatSPKI,
  RsaKeyPairGenJob,
  RSA_PKCS1_PSS_PADDING,
} = internalBinding('crypto');

const {
  validateInt32,
} = require('internal/validators');

const {
  bigIntArrayToUnsignedInt,
  getDigestSizeInBytes,
  getUsagesMask,
  jobPromise,
  normalizeHashName,
  validateMaxBufferLength,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const {
  InternalCryptoKey,
  getCryptoKeyAlgorithm,
  getCryptoKeyHandle,
  getCryptoKeyType,
} = require('internal/crypto/keys');

const {
  createKeyUsages,
  getKeyPairUsages,
  importDerKey,
  importJwkKey,
  validateJwk,
  validateKeyUsages,
  validateUsagesNotEmpty,
  verifyAcceptableKeyUse,
} = require('internal/crypto/webcrypto_util');

const kOaepUsages = createKeyUsages(
  ['encrypt', 'wrapKey'],
  ['decrypt', 'unwrapKey']);

const kSignVerifyUsages = createKeyUsages(['verify'], ['sign']);

const kUsages = {
  '__proto__': null,
  'RSA-OAEP': kOaepUsages,
  'RSA-PSS': kSignVerifyUsages,
  'RSASSA-PKCS1-v1_5': kSignVerifyUsages,
};

function validateRsaOaepAlgorithm(algorithm) {
  if (algorithm.label !== undefined) {
    validateMaxBufferLength(algorithm.label, 'algorithm.label');
  }
}

function rsaOaepCipher(mode, key, data, algorithm) {
  validateRsaOaepAlgorithm(algorithm);

  const type = mode === kWebCryptoCipherEncrypt ? 'public' : 'private';
  if (getCryptoKeyType(key) !== type) {
    throw lazyDOMException(
      'The requested operation is not valid for the provided key',
      'InvalidAccessError');
  }

  return jobPromise(() => new RSACipherJob(
    kCryptoJobWebCrypto,
    mode,
    getCryptoKeyHandle(key),
    data,
    kKeyVariantRSA_OAEP,
    normalizeHashName(getCryptoKeyAlgorithm(key).hash.name),
    algorithm.label));
}

function rsaKeyGenerate(
  algorithm,
  extractable,
  usages,
) {
  const publicExponentConverted = bigIntArrayToUnsignedInt(algorithm.publicExponent);
  if (publicExponentConverted === undefined) {
    throw lazyDOMException(
      'The publicExponent must be equivalent to an unsigned 32-bit value',
      'OperationError');
  }
  const {
    name,
    modulusLength,
    publicExponent,
    hash,
  } = algorithm;

  const allowedUsages = kUsages[name];
  const usagesSet = validateKeyUsages(usages, allowedUsages.keygen, name);

  const keyAlgorithm = {
    name,
    modulusLength,
    publicExponent,
    hash,
  };

  if (publicExponentConverted < 3 || publicExponentConverted % 2 === 0) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      'OperationError');
  }

  const keyUsages = getKeyPairUsages(usagesSet, allowedUsages);
  validateUsagesNotEmpty(keyUsages.private);

  return jobPromise(() => new RsaKeyPairGenJob(
    kCryptoJobWebCrypto,
    kKeyVariantRSA_SSA_PKCS1_v1_5,
    modulusLength,
    publicExponentConverted,
    keyAlgorithm,
    getUsagesMask(keyUsages.public),
    getUsagesMask(keyUsages.private),
    extractable));
}

function rsaExportKey(key, format) {
  try {
    switch (format) {
      case kWebCryptoKeyFormatSPKI: {
        return TypedArrayPrototypeGetBuffer(
          getCryptoKeyHandle(key).export(kKeyFormatDER, kWebCryptoKeyFormatSPKI));
      }
      case kWebCryptoKeyFormatPKCS8: {
        return TypedArrayPrototypeGetBuffer(
          getCryptoKeyHandle(key).export(kKeyFormatDER, kWebCryptoKeyFormatPKCS8, null, null));
      }
      default:
        return undefined;
    }
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  }
}

function rsaImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  usages) {
  const allowedUsages = kUsages[algorithm.name];
  const usagesSet = new SafeSet(usages);
  let handle;
  switch (format) {
    case 'KeyObjectHandle':
      verifyAcceptableKeyUse(
        algorithm.name,
        usagesSet,
        keyData.getKeyType() === kKeyTypePublic ?
          allowedUsages.public :
          allowedUsages.private);
      handle = keyData;
      break;
    case 'spki': {
      verifyAcceptableKeyUse(
        algorithm.name, usagesSet, allowedUsages.public);
      handle = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableKeyUse(
        algorithm.name, usagesSet, allowedUsages.private);
      handle = importDerKey(keyData, false);
      break;
    }
    case 'jwk': {
      const expectedUse = algorithm.name === 'RSA-OAEP' ? 'enc' : 'sig';
      validateJwk(keyData, 'RSA', extractable, usagesSet, expectedUse);

      if (keyData.alg !== undefined) {
        const expected =
          normalizeHashName(algorithm.hash.name,
                            algorithm.name === 'RSASSA-PKCS1-v1_5' ? normalizeHashName.kContextJwkRsa :
                              algorithm.name === 'RSA-PSS' ? normalizeHashName.kContextJwkRsaPss :
                                normalizeHashName.kContextJwkRsaOaep);

        if (expected && keyData.alg !== expected)
          throw lazyDOMException(
            'JWK "alg" does not match the requested algorithm',
            'DataError');
      }

      const isPublic = keyData.d === undefined;
      verifyAcceptableKeyUse(
        algorithm.name,
        usagesSet,
        isPublic ? allowedUsages.public : allowedUsages.private);
      handle = importJwkKey(isPublic, keyData);
      break;
    }
    default:
      return undefined;
  }

  if (handle.getAsymmetricKeyType() !== 'rsa') {
    throw lazyDOMException('Invalid key type', 'DataError');
  }

  const {
    modulusLength,
    publicExponent,
  } = handle.keyDetail({});

  return new InternalCryptoKey(handle, {
    name: algorithm.name,
    modulusLength,
    publicExponent: new Uint8Array(publicExponent),
    hash: algorithm.hash,
  }, getUsagesMask(usagesSet), extractable);
}

function rsaSignVerify(key, data, { saltLength }, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (getCryptoKeyType(key) !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  const algorithm = getCryptoKeyAlgorithm(key);
  if (algorithm.name === 'RSA-PSS') {
    try {
      validateInt32(
        saltLength,
        'algorithm.saltLength',
        0,
        MathCeil((algorithm.modulusLength - 1) / 8) -
          getDigestSizeInBytes(algorithm.hash.name) - 2);
    } catch (err) {
      throw lazyDOMException(
        'The operation failed for an operation-specific reason',
        { name: 'OperationError', cause: err });
    }
  }

  return jobPromise(() => new SignJob(
    kCryptoJobWebCrypto,
    signature === undefined ? kSignJobModeSign : kSignJobModeVerify,
    getCryptoKeyHandle(key),
    undefined,
    undefined,
    undefined,
    undefined,
    data,
    normalizeHashName(algorithm.hash.name),
    saltLength,
    algorithm.name === 'RSA-PSS' ? RSA_PKCS1_PSS_PADDING : undefined,
    undefined,
    undefined,
    signature));
}


module.exports = {
  rsaCipher: rsaOaepCipher,
  rsaExportKey,
  rsaImportKey,
  rsaKeyGenerate,
  rsaSignVerify,
};
