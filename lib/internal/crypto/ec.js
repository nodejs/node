'use strict';

const {
  SafeSet,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  EcKeyPairGenJob,
  KeyObjectHandle,
  SignJob,
  kCryptoJobWebCrypto,
  kKeyFormatDER,
  kKeyFormatRawPublic,
  kKeyTypePublic,
  kSignJobModeSign,
  kSignJobModeVerify,
  kSigEncP1363,
  kWebCryptoKeyFormatPKCS8,
  kWebCryptoKeyFormatRaw,
  kWebCryptoKeyFormatSPKI,
} = internalBinding('crypto');

const {
  crypto: {
    POINT_CONVERSION_UNCOMPRESSED,
  },
} = internalBinding('constants');

const {
  getUsagesMask,
  jobPromise,
  normalizeHashName,
  kNamedCurveAliases,
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
  importRawKey,
  validateJwk,
  validateKeyUsages,
  validateUsagesNotEmpty,
  verifyAcceptableKeyUse,
} = require('internal/crypto/webcrypto_util');

const kDeriveUsages = createKeyUsages([], ['deriveKey', 'deriveBits']);

const kSignVerifyUsages = createKeyUsages(['verify'], ['sign']);

const kUsages = {
  '__proto__': null,
  'ECDH': kDeriveUsages,
  'ECDSA': kSignVerifyUsages,
};

function ecGenerateKey(algorithm, extractable, usages) {
  const { name, namedCurve } = algorithm;
  const allowedUsages = kUsages[name];
  const usagesSet = validateKeyUsages(usages, allowedUsages.keygen, name);

  const keyAlgorithm = { name, namedCurve };
  const keyUsages = getKeyPairUsages(usagesSet, allowedUsages);
  validateUsagesNotEmpty(keyUsages.private);

  return jobPromise(() => new EcKeyPairGenJob(
    kCryptoJobWebCrypto,
    namedCurve,
    undefined,
    keyAlgorithm,
    getUsagesMask(keyUsages.public),
    getUsagesMask(keyUsages.private),
    extractable));
}

function ecExportKey(key, format) {
  try {
    const handle = getCryptoKeyHandle(key);
    switch (format) {
      case kWebCryptoKeyFormatRaw: {
        return TypedArrayPrototypeGetBuffer(
          handle.exportECPublicRaw(POINT_CONVERSION_UNCOMPRESSED));
      }
      case kWebCryptoKeyFormatSPKI: {
        let spki = handle.export(kKeyFormatDER, kWebCryptoKeyFormatSPKI);
        // WebCrypto requires uncompressed point format for SPKI exports.
        // This is a very rare edge case dependent on the imported key
        // using compressed point format.
        // Expected SPKI DER byte lengths with uncompressed points:
        // P-256: 91 = 26 bytes of SPKI ASN.1 + 65-byte uncompressed point.
        // P-384: 120 = 23 bytes of SPKI ASN.1 + 97-byte uncompressed point.
        // P-521: 158 = 25 bytes of SPKI ASN.1 + 133-byte uncompressed point.
        // Difference in initial SPKI ASN.1 is caused by OIDs and length encoding.
        const { namedCurve } = getCryptoKeyAlgorithm(key);
        if (TypedArrayPrototypeGetByteLength(spki) !== {
          '__proto__': null, 'P-256': 91, 'P-384': 120, 'P-521': 158,
        }[namedCurve]) {
          const raw = handle.exportECPublicRaw(POINT_CONVERSION_UNCOMPRESSED);
          const tmp = new KeyObjectHandle();
          tmp.init(kKeyTypePublic, raw, kKeyFormatRawPublic,
                   'ec', null, namedCurve);
          spki = tmp.export(kKeyFormatDER, kWebCryptoKeyFormatSPKI);
        }
        return TypedArrayPrototypeGetBuffer(spki);
      }
      case kWebCryptoKeyFormatPKCS8: {
        return TypedArrayPrototypeGetBuffer(
          handle.export(kKeyFormatDER, kWebCryptoKeyFormatPKCS8, null, null));
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

function ecImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  usages,
) {
  const { name, namedCurve } = algorithm;

  let handle;
  const allowedUsages = kUsages[name];
  const usagesSet = new SafeSet(usages);
  switch (format) {
    case 'KeyObjectHandle':
      verifyAcceptableKeyUse(
        name,
        usagesSet,
        keyData.getKeyType() === kKeyTypePublic ?
          allowedUsages.public :
          allowedUsages.private);
      handle = keyData;
      break;
    case 'spki': {
      verifyAcceptableKeyUse(name, usagesSet, allowedUsages.public);
      handle = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableKeyUse(name, usagesSet, allowedUsages.private);
      handle = importDerKey(keyData, false);
      break;
    }
    case 'jwk': {
      const expectedUse = name === 'ECDH' ? 'enc' : 'sig';
      validateJwk(keyData, 'EC', extractable, usagesSet, expectedUse);

      if (keyData.crv !== namedCurve)
        throw lazyDOMException(
          'JWK "crv" does not match the requested algorithm',
          'DataError');

      if (algorithm.name === 'ECDSA' && keyData.alg !== undefined) {
        let algNamedCurve;
        switch (keyData.alg) {
          case 'ES256': algNamedCurve = 'P-256'; break;
          case 'ES384': algNamedCurve = 'P-384'; break;
          case 'ES512': algNamedCurve = 'P-521'; break;
        }
        if (algNamedCurve !== namedCurve)
          throw lazyDOMException(
            'JWK "alg" does not match the requested algorithm',
            'DataError');
      }

      const isPublic = keyData.d === undefined;
      verifyAcceptableKeyUse(
        name,
        usagesSet,
        isPublic ? allowedUsages.public : allowedUsages.private);
      handle = importJwkKey(isPublic, keyData);
      break;
    }
    case 'raw': {
      verifyAcceptableKeyUse(name, usagesSet, allowedUsages.public);
      handle = importRawKey(true, keyData, kKeyFormatRawPublic, 'ec', namedCurve);
      break;
    }
    default:
      return undefined;
  }

  switch (algorithm.name) {
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      if (handle.getAsymmetricKeyType() !== 'ec')
        throw lazyDOMException('Invalid key type', 'DataError');
      break;
  }

  if (!handle.checkEcKeyData()) {
    throw lazyDOMException('Invalid keyData', 'DataError');
  }

  if (kNamedCurveAliases[namedCurve] !== handle.keyDetail({}).namedCurve)
    throw lazyDOMException('Named curve mismatch', 'DataError');

  return new InternalCryptoKey(
    handle,
    { name, namedCurve },
    getUsagesMask(usagesSet),
    extractable);
}

function ecdsaSignVerify(key, data, { name, hash }, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (getCryptoKeyType(key) !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  return jobPromise(() => new SignJob(
    kCryptoJobWebCrypto,
    mode,
    getCryptoKeyHandle(key),
    undefined,
    undefined,
    undefined,
    undefined,
    data,
    normalizeHashName(hash.name),
    undefined,  // Salt length, not used with ECDSA
    undefined,  // PSS Padding, not used with ECDSA
    kSigEncP1363,
    undefined,
    signature));
}

module.exports = {
  ecExportKey,
  ecImportKey,
  ecGenerateKey,
  ecdsaSignVerify,
};
