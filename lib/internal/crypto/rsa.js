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
  kCryptoJobAsync,
  kKeyFormatDER,
  kSignJobModeSign,
  kSignJobModeVerify,
  kKeyVariantRSA_OAEP,
  kWebCryptoCipherEncrypt,
  kWebCryptoKeyFormatPKCS8,
  kWebCryptoKeyFormatSPKI,
  RSA_PKCS1_PSS_PADDING,
} = internalBinding('crypto');

const {
  validateInt32,
} = require('internal/validators');

const {
  bigIntArrayToUnsignedInt,
  getDigestSizeInBytes,
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
  normalizeHashName,
  validateMaxBufferLength,
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  lazyDOMException,
  promisify,
} = require('internal/util');

const {
  InternalCryptoKey,
  kAlgorithm,
  kKeyType,
} = require('internal/crypto/keys');

const {
  importDerKey,
  importJwkKey,
  validateJwk,
} = require('internal/crypto/webcrypto_util');

const {
  generateKeyPair: _generateKeyPair,
} = require('internal/crypto/keygen');

const generateKeyPair = promisify(_generateKeyPair);

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

async function rsaOaepCipher(mode, key, data, algorithm) {
  validateRsaOaepAlgorithm(algorithm);

  const type = mode === kWebCryptoCipherEncrypt ? 'public' : 'private';
  if (key[kKeyType] !== type) {
    throw lazyDOMException(
      'The requested operation is not valid for the provided key',
      'InvalidAccessError');
  }

  return await jobPromise(() => new RSACipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    kKeyVariantRSA_OAEP,
    normalizeHashName(key[kAlgorithm].hash.name),
    algorithm.label));
}

async function rsaKeyGenerate(
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

  let keyPair;
  try {
    keyPair = await generateKeyPair('rsa', {
      modulusLength,
      publicExponent: publicExponentConverted,
    });
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  }

  const keyAlgorithm = {
    name,
    modulusLength,
    publicExponent,
    hash,
  };

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

  const publicKey =
    new InternalCryptoKey(
      keyPair.publicKey,
      keyAlgorithm,
      publicUsages,
      true);

  const privateKey =
    new InternalCryptoKey(
      keyPair.privateKey,
      keyAlgorithm,
      privateUsages,
      extractable);

  return { __proto__: null, publicKey, privateKey };
}

function rsaExportKey(key, format) {
  try {
    switch (format) {
      case kWebCryptoKeyFormatSPKI: {
        return TypedArrayPrototypeGetBuffer(
          key[kKeyObject][kHandle].export(kKeyFormatDER, kWebCryptoKeyFormatSPKI));
      }
      case kWebCryptoKeyFormatPKCS8: {
        return TypedArrayPrototypeGetBuffer(
          key[kKeyObject][kHandle].export(kKeyFormatDER, kWebCryptoKeyFormatPKCS8, null, null));
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
  let keyObject;
  switch (format) {
    case 'KeyObject': {
      verifyAcceptableRsaKeyUse(algorithm.name, keyData.type === 'public', usagesSet);
      keyObject = keyData;
      break;
    }
    case 'spki': {
      verifyAcceptableRsaKeyUse(algorithm.name, true, usagesSet);
      keyObject = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableRsaKeyUse(algorithm.name, false, usagesSet);
      keyObject = importDerKey(keyData, false);
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
      keyObject = importJwkKey(isPublic, keyData);
      break;
    }
    default:
      return undefined;
  }

  if (keyObject.asymmetricKeyType !== 'rsa') {
    throw lazyDOMException('Invalid key type', 'DataError');
  }

  const {
    modulusLength,
    publicExponent,
  } = keyObject[kHandle].keyDetail({});

  return new InternalCryptoKey(keyObject, {
    name: algorithm.name,
    modulusLength,
    publicExponent: new Uint8Array(publicExponent),
    hash: algorithm.hash,
  }, usagesSet, extractable);
}

async function rsaSignVerify(key, data, { saltLength }, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (key[kKeyType] !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  return await jobPromise(() => {
    if (key[kAlgorithm].name === 'RSA-PSS') {
      validateInt32(
        saltLength,
        'algorithm.saltLength',
        0,
        MathCeil((key[kAlgorithm].modulusLength - 1) / 8) - getDigestSizeInBytes(key[kAlgorithm].hash.name) - 2);
    }

    return new SignJob(
      kCryptoJobAsync,
      signature === undefined ? kSignJobModeSign : kSignJobModeVerify,
      key[kKeyObject][kHandle],
      undefined,
      undefined,
      undefined,
      undefined,
      data,
      normalizeHashName(key[kAlgorithm].hash.name),
      saltLength,
      key[kAlgorithm].name === 'RSA-PSS' ? RSA_PKCS1_PSS_PADDING : undefined,
      undefined,
      undefined,
      signature);
  });
}


module.exports = {
  rsaCipher: rsaOaepCipher,
  rsaExportKey,
  rsaImportKey,
  rsaKeyGenerate,
  rsaSignVerify,
};
