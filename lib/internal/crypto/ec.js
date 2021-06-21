'use strict';

const {
  ObjectKeys,
  Promise,
  SafeSet,
} = primordials;

const { Buffer } = require('buffer');

const {
  ECKeyExportJob,
  KeyObjectHandle,
  SignJob,
  kCryptoJobAsync,
  kKeyTypePrivate,
  kKeyTypePublic,
  kSignJobModeSign,
  kSignJobModeVerify,
  kSigEncP1363,
} = internalBinding('crypto');

const {
  validateBoolean,
  validateOneOf,
  validateString,
} = require('internal/validators');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
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
  isKeyObject,
} = require('internal/crypto/keys');

function verifyAcceptableEcKeyUse(name, type, usages) {
  let checkSet;
  switch (name) {
    case 'ECDH':
      checkSet = ['deriveKey', 'deriveBits'];
      break;
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
      // Fall through
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

function createECRawKey(namedCurve, keyData, isPublic) {
  const handle = new KeyObjectHandle();
  keyData = getArrayBufferOrView(keyData, 'keyData');

  switch (namedCurve) {
    case 'NODE-ED25519':
    case 'NODE-X25519':
      if (keyData.byteLength !== 32) {
        throw lazyDOMException(
          `${namedCurve} raw keys must be exactly 32-bytes`);
      }
      break;
    case 'NODE-ED448':
      if (keyData.byteLength !== 57) {
        throw lazyDOMException(
          `${namedCurve} raw keys must be exactly 57-bytes`);
      }
      break;
    case 'NODE-X448':
      if (keyData.byteLength !== 56) {
        throw lazyDOMException(
          `${namedCurve} raw keys must be exactly 56-bytes`);
      }
      break;
  }

  if (isPublic) {
    handle.initEDRaw(namedCurve, keyData, kKeyTypePublic);
    return new PublicKeyObject(handle);
  }

  handle.initEDRaw(namedCurve, keyData, kKeyTypePrivate);
  return new PrivateKeyObject(handle);
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
      if (namedCurve === 'NODE-ED25519' ||
          namedCurve === 'NODE-ED448' ||
          namedCurve === 'NODE-X25519' ||
          namedCurve === 'NODE-X448') {
        throw lazyDOMException('Unsupported named curves for ECDSA');
      }
      // Fall through
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
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
      if (namedCurve === 'NODE-ED25519' || namedCurve === 'NODE-ED448') {
        throw lazyDOMException('Unsupported named curves for ECDH');
      }
      // Fall through
  }
  return new Promise((resolve, reject) => {
    let genKeyType;
    let genOpts;
    switch (namedCurve) {
      case 'NODE-ED25519':
        genKeyType = 'ed25519';
        break;
      case 'NODE-ED448':
        genKeyType = 'ed448';
        break;
      case 'NODE-X25519':
        genKeyType = 'x25519';
        break;
      case 'NODE-X448':
        genKeyType = 'x448';
        break;
      default:
        genKeyType = 'ec';
        genOpts = { namedCurve };
        break;
    }
    generateKeyPair(genKeyType, genOpts, (err, pubKey, privKey) => {
      if (err) {
        return reject(lazyDOMException(
          'The operation failed for an operation-specific reason',
          'OperationError'));
      }

      const algorithm = { name, namedCurve };

      let publicUsages;
      let privateUsages;
      switch (name) {
        case 'NODE-ED25519':
          // Fall through
        case 'NODE-ED448':
          // Fall through
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
  let checkNamedCurve = true;
  switch (format) {
    case 'node.keyObject': {
      if (!isKeyObject(keyData))
        throw new ERR_INVALID_ARG_TYPE('keyData', 'KeyObject', keyData);
      if (keyData.type === 'secret')
        throw lazyDOMException('Invalid key type', 'InvalidAccessException');

      switch (namedCurve) {
        case 'NODE-X25519':
          // Fall through
        case 'NODE-X448':
          checkNamedCurve = false;
          if (algorithm.name !== 'ECDH')
            throw lazyDOMException('Invalid algorithm name.', 'DataError');
          break;
        case 'NODE-ED25519':
          // Fall through
        case 'NODE-ED448':
          checkNamedCurve = false;
          if (algorithm.name !== namedCurve)
            throw lazyDOMException('Invalid algorithm name.', 'DataError');
          break;
      }

      verifyAcceptableEcKeyUse(name, keyData.type, usagesSet);
      keyObject = keyData;
      break;
    }
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
      let curve;
      if (keyData == null || typeof keyData !== 'object')
        throw lazyDOMException('Invalid JWK keyData', 'DataError');
      switch (keyData.kty) {
        case 'OKP': {
          checkNamedCurve = false;
          const isPublic = keyData.d === undefined;

          let type;
          switch (namedCurve) {
            case 'NODE-ED25519':
              // Fall through
            case 'NODE-ED448':
              type = `NODE-${keyData.crv.toUpperCase()}`;
              break;
            case 'NODE-X25519':
              // Fall through
            case 'NODE-X448':
              type = 'ECDH';
              break;
          }

          if (algorithm.name !== type)
            throw lazyDOMException('Invalid algorithm name.', 'DataError');

          verifyAcceptableEcKeyUse(
            type,
            isPublic ? 'public' : 'private',
            usagesSet);
          keyObject = createECRawKey(
            namedCurve,
            Buffer.from(
              isPublic ? keyData.x : keyData.d,
              'base64'),
            isPublic);
          break;
        }
        default: {
          if (keyData.kty !== 'EC')
            throw lazyDOMException('Invalid key type', 'DataError');

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
            switch (keyData.alg) {
              case 'ES256': curve = 'P-256'; break;
              case 'ES384': curve = 'P-384'; break;
              case 'ES512': curve = 'P-521'; break;
            }
            if (curve !== namedCurve)
              throw lazyDOMException('Named curve mismatch', 'DataError');
          }

          const handle = new KeyObjectHandle();
          const type = handle.initJwk(keyData, namedCurve);
          if (type === undefined)
            throw lazyDOMException('Invalid JWK keyData', 'DataError');
          keyObject = type === kKeyTypePrivate ?
            new PrivateKeyObject(handle) :
            new PublicKeyObject(handle);
        }
      }
      break;
    }
    case 'raw': {
      switch (namedCurve) {
        case 'NODE-X25519':
          // Fall through
        case 'NODE-X448':
          checkNamedCurve = false;
          if (algorithm.public !== undefined)
            validateBoolean(algorithm.public, 'algorithm.public');
          if (algorithm.name !== 'ECDH')
            throw lazyDOMException('Invalid algorithm name.', 'DataError');
          verifyAcceptableEcKeyUse(
            algorithm.name,
            algorithm.public === true ? 'public' : 'private',
            usagesSet);
          keyObject = createECRawKey(namedCurve, keyData, algorithm.public);
          break;
        case 'NODE-ED25519':
          // Fall through
        case 'NODE-ED448':
          checkNamedCurve = false;
          if (algorithm.public !== undefined)
            validateBoolean(algorithm.public, 'algorithm.public');
          if (algorithm.name !== namedCurve)
            throw lazyDOMException('Invalid algorithm name.', 'DataError');
          verifyAcceptableEcKeyUse(
            algorithm.name,
            algorithm.public === true ? 'public' : 'private',
            usagesSet);
          keyObject = createECRawKey(namedCurve, keyData, algorithm.public);
          break;
        default:
          verifyAcceptableEcKeyUse(name, 'public', usagesSet);
          keyObject = createECPublicKeyRaw(namedCurve, keyData);
      }
      if (keyObject === undefined)
        throw lazyDOMException('Unable to import EC key', 'OperationError');
      break;
    }
  }

  if (checkNamedCurve) {
    const {
      namedCurve: checkNamedCurve
    } = keyObject[kHandle].keyDetail({});
    if (kNamedCurveAliases[namedCurve] !== checkNamedCurve)
      throw lazyDOMException('Named curve mismatch', 'DataError');
  }

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

  let hashname;
  switch (name) {
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
      if (hash !== undefined)
        throw new lazyDOMException(`Hash is not permitted for ${name}`);
      break;
    default:
      if (hash === undefined)
        throw new ERR_MISSING_OPTION('algorithm.hash');
      hashname = normalizeHashName(hash.name);
  }

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
