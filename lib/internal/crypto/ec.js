'use strict';

const {
  SafeSet,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  KeyObjectHandle,
  SignJob,
  kCryptoJobAsync,
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
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
  normalizeHashName,
  kHandle,
  kKeyObject,
  kNamedCurveAliases,
} = require('internal/crypto/util');

const {
  lazyDOMException,
  promisify,
} = require('internal/util');

const {
  generateKeyPair: _generateKeyPair,
} = require('internal/crypto/keygen');

const {
  InternalCryptoKey,
  kAlgorithm,
  kKeyType,
} = require('internal/crypto/keys');

const {
  importDerKey,
  importJwkKey,
  importRawKey,
  validateJwk,
} = require('internal/crypto/webcrypto_util');

const generateKeyPair = promisify(_generateKeyPair);

function verifyAcceptableEcKeyUse(name, isPublic, usages) {
  let checkSet;
  switch (name) {
    case 'ECDH':
      checkSet = isPublic ? [] : ['deriveKey', 'deriveBits'];
      break;
    case 'ECDSA':
      checkSet = isPublic ? ['verify'] : ['sign'];
      break;
    default:
      throw lazyDOMException(
        'The algorithm is not supported', 'NotSupportedError');
  }
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for a ${name} key`,
      'SyntaxError');
  }
}

async function ecGenerateKey(algorithm, extractable, keyUsages) {
  const { name, namedCurve } = algorithm;

  const usageSet = new SafeSet(keyUsages);
  switch (name) {
    case 'ECDSA':
      if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
        throw lazyDOMException(
          'Unsupported key usage for an ECDSA key',
          'SyntaxError');
      }
      break;
    case 'ECDH':
      if (hasAnyNotIn(usageSet, ['deriveKey', 'deriveBits'])) {
        throw lazyDOMException(
          'Unsupported key usage for an ECDH key',
          'SyntaxError');
      }
      // Fall through
  }

  let keyPair;
  try {
    keyPair = await generateKeyPair('ec', { namedCurve });
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  }

  let publicUsages;
  let privateUsages;
  switch (name) {
    case 'ECDSA':
      publicUsages = getUsagesUnion(usageSet, 'verify');
      privateUsages = getUsagesUnion(usageSet, 'sign');
      break;
    case 'ECDH':
      publicUsages = [];
      privateUsages = getUsagesUnion(usageSet, 'deriveKey', 'deriveBits');
      break;
  }

  const keyAlgorithm = { name, namedCurve };

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

function ecExportKey(key, format) {
  try {
    const handle = key[kKeyObject][kHandle];
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
        if (TypedArrayPrototypeGetByteLength(spki) !== {
          '__proto__': null, 'P-256': 91, 'P-384': 120, 'P-521': 158,
        }[key[kAlgorithm].namedCurve]) {
          const raw = handle.exportECPublicRaw(POINT_CONVERSION_UNCOMPRESSED);
          const tmp = new KeyObjectHandle();
          tmp.init(kKeyTypePublic, raw, kKeyFormatRawPublic,
                   'ec', null, key[kAlgorithm].namedCurve);
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
  keyUsages,
) {
  const { name, namedCurve } = algorithm;

  let keyObject;
  const usagesSet = new SafeSet(keyUsages);
  switch (format) {
    case 'KeyObject': {
      verifyAcceptableEcKeyUse(name, keyData.type === 'public', usagesSet);
      keyObject = keyData;
      break;
    }
    case 'spki': {
      verifyAcceptableEcKeyUse(name, true, usagesSet);
      keyObject = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableEcKeyUse(name, false, usagesSet);
      keyObject = importDerKey(keyData, false);
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
      verifyAcceptableEcKeyUse(name, isPublic, usagesSet);
      keyObject = importJwkKey(isPublic, keyData);
      break;
    }
    case 'raw': {
      verifyAcceptableEcKeyUse(name, true, usagesSet);
      keyObject = importRawKey(true, keyData, kKeyFormatRawPublic, 'ec', namedCurve);
      break;
    }
    default:
      return undefined;
  }

  switch (algorithm.name) {
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      if (keyObject.asymmetricKeyType !== 'ec')
        throw lazyDOMException('Invalid key type', 'DataError');
      break;
  }

  if (!keyObject[kHandle].checkEcKeyData()) {
    throw lazyDOMException('Invalid keyData', 'DataError');
  }

  const {
    namedCurve: checkNamedCurve,
  } = keyObject[kHandle].keyDetail({});
  if (kNamedCurveAliases[namedCurve] !== checkNamedCurve)
    throw lazyDOMException('Named curve mismatch', 'DataError');

  return new InternalCryptoKey(
    keyObject,
    { name, namedCurve },
    keyUsages,
    extractable);
}

async function ecdsaSignVerify(key, data, { name, hash }, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (key[kKeyType] !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  const hashname = normalizeHashName(hash.name);

  return await jobPromise(() => new SignJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    undefined,
    undefined,
    undefined,
    undefined,
    data,
    hashname,
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
