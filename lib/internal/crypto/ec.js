'use strict';

const {
  ObjectKeys,
  Promise,
  SafeSet,
} = primordials;

const {
  ECKeyExportJob,
  KeyObjectHandle,
  SignJob,
  kCryptoJobAsync,
  kKeyTypePrivate,
  kSignJobModeSign,
  kSignJobModeVerify,
  kSigEncP1363,
} = internalBinding('crypto');

const {
  validateOneOf,
  validateString,
} = require('internal/validators');

const {
  codes: {
    ERR_MISSING_OPTION,
  }
} = require('internal/errors');

const {
  getArrayBufferOrView,
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
  normalizeHashName,
  validateKeyOps,
  kHandle,
  kKeyObject,
  kNamedCurveAliases,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const {
  generateKeyPair,
} = require('internal/crypto/keygen');

const {
  InternalCryptoKey,
  PrivateKeyObject,
  PublicKeyObject,
  createPrivateKey,
  createPublicKey,
} = require('internal/crypto/keys');

function verifyAcceptableEcKeyUse(name, type, usages) {
  let checkSet;
  switch (name) {
    case 'ECDH':
      checkSet = ['deriveKey', 'deriveBits'];
      break;
    case 'ECDSA':
      switch (type) {
        case 'private':
          checkSet = ['sign'];
          break;
        case 'public':
          checkSet = ['verify'];
          break;
      }
  }
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for a ${name} key`,
      'SyntaxError');
  }
}

function createECPublicKeyRaw(namedCurve, keyData) {
  const handle = new KeyObjectHandle();
  keyData = getArrayBufferOrView(keyData, 'keyData');
  if (handle.initECRaw(kNamedCurveAliases[namedCurve], keyData))
    return new PublicKeyObject(handle);
}

async function ecGenerateKey(algorithm, extractable, keyUsages) {
  const { name, namedCurve } = algorithm;
  validateString(namedCurve, 'algorithm.namedCurve');
  validateOneOf(
    namedCurve,
    'algorithm.namedCurve',
    ObjectKeys(kNamedCurveAliases));

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
  return new Promise((resolve, reject) => {
    generateKeyPair('ec', { namedCurve }, (err, pubKey, privKey) => {
      if (err) {
        return reject(lazyDOMException(
          'The operation failed for an operation-specific reason',
          'OperationError'));
      }

      const algorithm = { name, namedCurve };

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

      const publicKey =
        new InternalCryptoKey(
          pubKey,
          algorithm,
          publicUsages,
          true);

      const privateKey =
        new InternalCryptoKey(
          privKey,
          algorithm,
          privateUsages,
          extractable);

      resolve({ publicKey, privateKey });
    });
  });
}

function ecExportKey(key, format) {
  return jobPromise(new ECKeyExportJob(
    kCryptoJobAsync,
    format,
    key[kKeyObject][kHandle]));
}

async function ecImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages) {

  const { name, namedCurve } = algorithm;
  validateString(namedCurve, 'algorithm.namedCurve');
  validateOneOf(
    namedCurve,
    'algorithm.namedCurve',
    ObjectKeys(kNamedCurveAliases));
  let keyObject;
  const usagesSet = new SafeSet(keyUsages);
  switch (format) {
    case 'spki': {
      verifyAcceptableEcKeyUse(name, 'public', usagesSet);
      keyObject = createPublicKey({
        key: keyData,
        format: 'der',
        type: 'spki'
      });
      break;
    }
    case 'pkcs8': {
      verifyAcceptableEcKeyUse(name, 'private', usagesSet);
      keyObject = createPrivateKey({
        key: keyData,
        format: 'der',
        type: 'pkcs8'
      });
      break;
    }
    case 'jwk': {
      if (keyData == null || typeof keyData !== 'object')
        throw lazyDOMException('Invalid JWK keyData', 'DataError');
      if (keyData.kty !== 'EC')
        throw lazyDOMException('Invalid key type', 'DataError');
      if (keyData.crv !== namedCurve)
        throw lazyDOMException('Named curve mismatch', 'DataError');

      if (keyData.d !== undefined) {
        verifyAcceptableEcKeyUse(name, 'private', usagesSet);
      } else {
        verifyAcceptableEcKeyUse(name, 'public', usagesSet);
      }

      if (usagesSet.size > 0 && keyData.use !== undefined) {
        if (algorithm.name === 'ECDSA' && keyData.use !== 'sig')
          throw lazyDOMException('Invalid use type', 'DataError');
        if (algorithm.name === 'ECDH' && keyData.use !== 'enc')
          throw lazyDOMException('Invalid use type', 'DataError');
      }

      validateKeyOps(keyData.key_ops, usagesSet);

      if (keyData.ext !== undefined &&
          keyData.ext === false &&
          extractable === true) {
        throw lazyDOMException('JWK is not extractable', 'DataError');
      }

      if (algorithm.name === 'ECDSA' && keyData.alg !== undefined) {
        if (typeof keyData.alg !== 'string')
          throw lazyDOMException('Invalid alg', 'DataError');
        let algNamedCurve;
        switch (keyData.alg) {
          case 'ES256': algNamedCurve = 'P-256'; break;
          case 'ES384': algNamedCurve = 'P-384'; break;
          case 'ES512': algNamedCurve = 'P-521'; break;
        }
        if (algNamedCurve !== namedCurve)
          throw lazyDOMException('Named curve mismatch', 'DataError');
      }

      const handle = new KeyObjectHandle();
      const type = handle.initJwk(keyData, namedCurve);
      if (type === undefined)
        throw lazyDOMException('Invalid JWK keyData', 'DataError');
      keyObject = type === kKeyTypePrivate ?
        new PrivateKeyObject(handle) :
        new PublicKeyObject(handle);
      break;
    }
    case 'raw': {
      verifyAcceptableEcKeyUse(name, 'public', usagesSet);
      keyObject = createECPublicKeyRaw(namedCurve, keyData);
      if (keyObject === undefined)
        throw lazyDOMException('Unable to import EC key', 'OperationError');
      break;
    }
  }

  switch (algorithm.name) {
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      if (keyObject.asymmetricKeyType !== 'ec')
        throw lazyDOMException('Invalid key type', 'DataError');
      break;
  }

  const {
    namedCurve: checkNamedCurve
  } = keyObject[kHandle].keyDetail({});
  if (kNamedCurveAliases[namedCurve] !== checkNamedCurve)
    throw lazyDOMException('Named curve mismatch', 'DataError');

  return new InternalCryptoKey(
    keyObject,
    { name, namedCurve },
    keyUsages,
    extractable);
}

function ecdsaSignVerify(key, data, { name, hash }, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (key.type !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  if (hash === undefined)
    throw new ERR_MISSING_OPTION('algorithm.hash');
  const hashname = normalizeHashName(hash.name);

  return jobPromise(new SignJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    undefined,
    undefined,
    undefined,
    data,
    hashname,
    undefined,  // Salt length, not used with ECDSA
    undefined,  // PSS Padding, not used with ECDSA
    kSigEncP1363,
    signature));
}

module.exports = {
  ecExportKey,
  ecImportKey,
  ecGenerateKey,
  ecdsaSignVerify,
};
