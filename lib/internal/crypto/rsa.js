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
  getUsagesUnion,
  hasAnyNotIn,
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
  getKeyObjectHandle,
  getKeyObjectType,
} = require('internal/crypto/keys');

const {
  importDerKey,
  importJwkKey,
  validateJwk,
} = require('internal/crypto/webcrypto_util');

function verifyAcceptableRsaKeyUse(name, isPublic, usages) {
  let checkSet;
  switch (name) {
    case 'RSA-OAEP':
      checkSet = isPublic ? ['encrypt', 'wrapKey'] : ['decrypt', 'unwrapKey'];
      break;
    case 'RSA-PSS':
      // Fall through
    case 'RSASSA-PKCS1-v1_5':
      checkSet = isPublic ? ['verify'] : ['sign'];
      break;
    default:
      throw lazyDOMException(
        'The algorithm is not supported', 'NotSupportedError');
  }
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for an ${name} key`,
      'SyntaxError');
  }
}

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
  keyUsages,
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

  const usageSet = new SafeSet(keyUsages);

  switch (name) {
    case 'RSA-OAEP':
      if (hasAnyNotIn(usageSet,
                      ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'])) {
        throw lazyDOMException(
          'Unsupported key usage for a RSA key',
          'SyntaxError');
      }
      break;
    default:
      if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
        throw lazyDOMException(
          'Unsupported key usage for a RSA key',
          'SyntaxError');
      }
  }

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

  let publicUsages;
  let privateUsages;
  switch (name) {
    case 'RSA-OAEP': {
      publicUsages = getUsagesUnion(usageSet, 'encrypt', 'wrapKey');
      privateUsages = getUsagesUnion(usageSet, 'decrypt', 'unwrapKey');
      break;
    }
    default: {
      publicUsages = getUsagesUnion(usageSet, 'verify');
      privateUsages = getUsagesUnion(usageSet, 'sign');
      break;
    }
  }

  if (privateUsages.size === 0) {
    throw lazyDOMException(
      'Usages cannot be empty when creating a key.',
      'SyntaxError');
  }

  return jobPromise(() => new RsaKeyPairGenJob(
    kCryptoJobWebCrypto,
    kKeyVariantRSA_SSA_PKCS1_v1_5,
    modulusLength,
    publicExponentConverted,
    keyAlgorithm,
    getUsagesMask(publicUsages),
    getUsagesMask(privateUsages),
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
  keyUsages) {
  const usagesSet = new SafeSet(keyUsages);
  let handle;
  switch (format) {
    case 'KeyObject': {
      verifyAcceptableRsaKeyUse(
        algorithm.name, getKeyObjectType(keyData) === 'public', usagesSet);
      handle = getKeyObjectHandle(keyData);
      break;
    }
    case 'spki': {
      verifyAcceptableRsaKeyUse(algorithm.name, true, usagesSet);
      handle = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableRsaKeyUse(algorithm.name, false, usagesSet);
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
      verifyAcceptableRsaKeyUse(algorithm.name, isPublic, usagesSet);
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
