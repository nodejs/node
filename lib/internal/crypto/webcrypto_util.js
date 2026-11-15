'use strict';

const {
  ArrayPrototypePushApply,
  SafeSet,
} = primordials;

const {
  KeyObjectHandle,
  kKeyFormatDER,
  kKeyFormatJWK,
  kKeyEncodingPKCS8,
  kKeyEncodingSPKI,
  kKeyTypePublic,
  kKeyTypePrivate,
  kKeyTypeSecret,
} = internalBinding('crypto');

const {
  hasAnyNotIn,
  validateKeyOps,
  toUsagesSet,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

/**
 * @typedef {object} KeyUsageLists
 * @property {string[]} public Usages accepted for public keys.
 * @property {string[]} private Usages accepted for private keys.
 * @property {string[]} keygen Usages accepted during key generation.
 */

/**
 * @typedef {object} KeyUsageSets
 * @property {SafeSet<string>} public Requested public key usages.
 * @property {SafeSet<string>} private Requested private key usages.
 */

/**
 * Validates that every requested key usage is allowed for `subject`.
 * @param {string} subject
 * @param {SafeSet<string>} usagesSet
 * @param {string[]} allowed
 */
function verifyAcceptableKeyUse(subject, usagesSet, allowed) {
  if (hasAnyNotIn(usagesSet, allowed)) {
    throw lazyDOMException(
      `Unsupported key usage for ${subject} key`,
      'SyntaxError');
  }
}

/**
 * Converts a usage list to a set and validates it against `allowed`.
 * @param {string[]} usages
 * @param {string[]} allowed
 * @param {string} subject
 * @returns {SafeSet<string>}
 */
function validateKeyUsages(usages, allowed, subject) {
  const usagesSet = toUsagesSet(usages);
  verifyAcceptableKeyUse(subject, usagesSet, allowed);
  return usagesSet;
}

/**
 * Validates that a usage set is not empty.
 * @param {SafeSet<string>} usagesSet
 * @returns {SafeSet<string>}
 */
function validateUsagesNotEmpty(usagesSet) {
  if (usagesSet.size === 0) {
    throw lazyDOMException(
      'Usages cannot be empty when creating a key.',
      'SyntaxError');
  }
  return usagesSet;
}

/**
 * Returns the requested usages that are present in `usagesSet`.
 * @param {SafeSet<string>} usagesSet
 * @param {string[]} usages
 * @returns {SafeSet<string>}
 */
function getUsagesUnion(usagesSet, usages) {
  const newset = new SafeSet();
  for (let n = 0; n < usages.length; n++) {
    if (usagesSet.has(usages[n]))
      newset.add(usages[n]);
  }
  return newset;
}

/**
 * Splits requested usages into public and private key usage sets.
 * @param {SafeSet<string>} usagesSet
 * @param {KeyUsageLists} allowed
 * @returns {KeyUsageSets}
 */
function getKeyPairUsages(usagesSet, allowed) {
  return {
    public: getUsagesUnion(usagesSet, allowed.public),
    private: getUsagesUnion(usagesSet, allowed.private),
  };
}

/**
 * Creates the accepted public, private, and key generation usage lists.
 * @param {string[]} publicUsages
 * @param {string[]} privateUsages
 * @returns {KeyUsageLists}
 */
function createKeyUsages(publicUsages, privateUsages) {
  const keygen = [];
  ArrayPrototypePushApply(keygen, publicUsages);
  ArrayPrototypePushApply(keygen, privateUsages);
  return {
    __proto__: null,
    public: publicUsages,
    private: privateUsages,
    keygen,
  };
}

function importDerKey(keyData, isPublic) {
  const handle = new KeyObjectHandle();
  const keyType = isPublic ? kKeyTypePublic : kKeyTypePrivate;
  const encoding = isPublic ? kKeyEncodingSPKI : kKeyEncodingPKCS8;
  try {
    handle.init(keyType, keyData, kKeyFormatDER, encoding, null, null);
  } catch (err) {
    throw lazyDOMException(
      'Invalid keyData', { name: 'DataError', cause: err });
  }
  return handle;
}

/**
 * Imports a key in one of the formats shared by the asymmetric WebCrypto
 * algorithms: 'KeyObjectHandle', 'spki', or 'pkcs8'.
 * @param {string} format
 * @param {any} keyData
 * @param {string} name The algorithm name.
 * @param {Set<string>} usagesSet
 * @param {{ public: string[], private: string[] }} allowedUsages
 * @param {(keyData: any) => void} [validatePkcs8] Additional validation run
 *   for 'pkcs8' key data after the key usages have been verified.
 * @returns {KeyObjectHandle}
 */
function importAsymmetricKeyHandle(
  format, keyData, name, usagesSet, allowedUsages, validatePkcs8) {
  switch (format) {
    case 'KeyObjectHandle':
      verifyAcceptableKeyUse(
        name,
        usagesSet,
        keyData.getKeyType() === kKeyTypePublic ?
          allowedUsages.public :
          allowedUsages.private);
      return keyData;
    case 'spki':
      verifyAcceptableKeyUse(name, usagesSet, allowedUsages.public);
      return importDerKey(keyData, true);
    case 'pkcs8':
      verifyAcceptableKeyUse(name, usagesSet, allowedUsages.private);
      validatePkcs8?.(keyData);
      return importDerKey(keyData, false);
  }
}

function validateJwk(keyData, kty, extractable, usagesSet, expectedUse) {
  if (typeof keyData.kty !== 'string')
    throw lazyDOMException('Invalid keyData', 'DataError');
  if (keyData.kty !== kty)
    throw lazyDOMException('Invalid JWK "kty" Parameter', 'DataError');
  switch (kty) {
    case 'RSA':
      if (typeof keyData.n !== 'string' ||
          typeof keyData.e !== 'string' ||
          (keyData.d !== undefined && typeof keyData.d !== 'string'))
        throw lazyDOMException('Invalid keyData', 'DataError');
      if (typeof keyData.d === 'string' &&
          (typeof keyData.p !== 'string' ||
           typeof keyData.q !== 'string' ||
           typeof keyData.dp !== 'string' ||
           typeof keyData.dq !== 'string' ||
           typeof keyData.qi !== 'string'))
        throw lazyDOMException('Invalid keyData', 'DataError');
      break;
    case 'EC':
      if (typeof keyData.crv !== 'string' ||
          typeof keyData.x !== 'string' ||
          typeof keyData.y !== 'string' ||
          (keyData.d !== undefined && typeof keyData.d !== 'string'))
        throw lazyDOMException('Invalid keyData', 'DataError');
      break;
    case 'OKP':
      if (typeof keyData.crv !== 'string' ||
          typeof keyData.x !== 'string' ||
          (keyData.d !== undefined && typeof keyData.d !== 'string'))
        throw lazyDOMException('Invalid keyData', 'DataError');
      break;
    case 'oct':
      if (typeof keyData.k !== 'string')
        throw lazyDOMException('Invalid keyData', 'DataError');
      break;
    case 'AKP':
      if (typeof keyData.alg !== 'string' ||
          typeof keyData.pub !== 'string' ||
          (keyData.priv !== undefined && typeof keyData.priv !== 'string'))
        throw lazyDOMException('Invalid keyData', 'DataError');
      break;
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }
  if (usagesSet.size > 0 && keyData.use !== undefined) {
    if (keyData.use !== expectedUse)
      throw lazyDOMException('Invalid JWK "use" Parameter', 'DataError');
  }
  validateKeyOps(keyData.key_ops, usagesSet);
  if (keyData.ext !== undefined &&
      keyData.ext === false &&
      extractable === true) {
    throw lazyDOMException(
      'JWK "ext" Parameter and extractable mismatch',
      'DataError');
  }
}

function importJwkKey(isPublic, keyData) {
  const handle = new KeyObjectHandle();
  const keyType = isPublic ? kKeyTypePublic : kKeyTypePrivate;
  try {
    handle.init(keyType, keyData, kKeyFormatJWK, null, null, null);
  } catch (err) {
    throw lazyDOMException(
      'Invalid keyData', { name: 'DataError', cause: err });
  }
  return handle;
}

function importRawKey(isPublic, keyData, format, name, namedCurve) {
  const handle = new KeyObjectHandle();
  const keyType = isPublic ? kKeyTypePublic : kKeyTypePrivate;
  try {
    handle.init(keyType, keyData, format, name ?? null, null, namedCurve ?? null);
  } catch (err) {
    throw lazyDOMException(
      'Invalid keyData', { name: 'DataError', cause: err });
  }
  return handle;
}

function importSecretKey(keyData) {
  const handle = new KeyObjectHandle();
  handle.init(kKeyTypeSecret, keyData);
  return handle;
}

function importJwkSecretKey(keyData) {
  const handle = new KeyObjectHandle();
  try {
    handle.init(kKeyTypeSecret, keyData, kKeyFormatJWK, null, null);
  } catch (err) {
    throw lazyDOMException(
      'Invalid keyData', { name: 'DataError', cause: err });
  }
  return handle;
}

module.exports = {
  createKeyUsages,
  getKeyPairUsages,
  importAsymmetricKeyHandle,
  importDerKey,
  importJwkKey,
  importJwkSecretKey,
  importRawKey,
  importSecretKey,
  validateJwk,
  validateKeyUsages,
  validateUsagesNotEmpty,
  verifyAcceptableKeyUse,
};
