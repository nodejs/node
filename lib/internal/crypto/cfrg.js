'use strict';

const {
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
} = internalBinding('crypto');

const {
  getArrayBufferOrView,
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
  validateKeyOps,
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  emitExperimentalWarning,
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

function verifyAcceptableCfrgKeyUse(name, type, usages) {
  let checkSet;
  switch (name) {
    case 'X25519':
      // Fall through
    case 'X448':
      checkSet = ['deriveKey', 'deriveBits'];
      break;
    case 'Ed25519':
      // Fall through
    case 'Ed448':
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

function createCFRGRawKey(name, keyData, isPublic) {
  const handle = new KeyObjectHandle();
  keyData = getArrayBufferOrView(keyData, 'keyData');

  switch (name) {
    case 'Ed25519':
    case 'X25519':
      if (keyData.byteLength !== 32) {
        throw lazyDOMException(
          `${name} raw keys must be exactly 32-bytes`);
      }
      break;
    case 'Ed448':
      if (keyData.byteLength !== 57) {
        throw lazyDOMException(
          `${name} raw keys must be exactly 57-bytes`);
      }
      break;
    case 'X448':
      if (keyData.byteLength !== 56) {
        throw lazyDOMException(
          `${name} raw keys must be exactly 56-bytes`);
      }
      break;
  }

  const keyType = isPublic ? kKeyTypePublic : kKeyTypePrivate;
  if (!handle.initEDRaw(name, keyData, keyType)) {
    throw lazyDOMException('Failure to generate key object');
  }

  return isPublic ? new PublicKeyObject(handle) : new PrivateKeyObject(handle);
}

async function cfrgGenerateKey(algorithm, extractable, keyUsages) {
  const { name } = algorithm;
  emitExperimentalWarning(`The ${name} Web Crypto API algorithm`);

  const usageSet = new SafeSet(keyUsages);
  switch (name) {
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
        throw lazyDOMException(
          `Unsupported key usage for an ${name} key`,
          'SyntaxError');
      }
      break;
    case 'X25519':
      // Fall through
    case 'X448':
      if (hasAnyNotIn(usageSet, ['deriveKey', 'deriveBits'])) {
        throw lazyDOMException(
          `Unsupported key usage for an ${name} key`,
          'SyntaxError');
      }
      break;
  }
  return new Promise((resolve, reject) => {
    let genKeyType;
    switch (name) {
      case 'Ed25519':
        genKeyType = 'ed25519';
        break;
      case 'Ed448':
        genKeyType = 'ed448';
        break;
      case 'X25519':
        genKeyType = 'x25519';
        break;
      case 'X448':
        genKeyType = 'x448';
        break;
    }
    generateKeyPair(genKeyType, undefined, (err, pubKey, privKey) => {
      if (err) {
        return reject(lazyDOMException(
          'The operation failed for an operation-specific reason',
          'OperationError'));
      }

      const algorithm = { name };

      let publicUsages;
      let privateUsages;
      switch (name) {
        case 'Ed25519':
          // Fall through
        case 'Ed448':
          publicUsages = getUsagesUnion(usageSet, 'verify');
          privateUsages = getUsagesUnion(usageSet, 'sign');
          break;
        case 'X25519':
          // Fall through
        case 'X448':
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

function cfrgExportKey(key, format) {
  emitExperimentalWarning(`The ${key.algorithm.name} Web Crypto API algorithm`);
  return jobPromise(new ECKeyExportJob(
    kCryptoJobAsync,
    format,
    key[kKeyObject][kHandle]));
}

async function cfrgImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages) {

  const { name } = algorithm;
  emitExperimentalWarning(`The ${name} Web Crypto API algorithm`);
  let keyObject;
  const usagesSet = new SafeSet(keyUsages);
  switch (format) {
    case 'spki': {
      verifyAcceptableCfrgKeyUse(name, 'public', usagesSet);
      keyObject = createPublicKey({
        key: keyData,
        format: 'der',
        type: 'spki'
      });
      break;
    }
    case 'pkcs8': {
      verifyAcceptableCfrgKeyUse(name, 'private', usagesSet);
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
      if (keyData.kty !== 'OKP')
        throw lazyDOMException('Invalid key type', 'DataError');
      if (keyData.crv !== name)
        throw lazyDOMException('Subtype mismatch', 'DataError');
      const isPublic = keyData.d === undefined;

      if (usagesSet.size > 0 && keyData.use !== undefined) {
        let checkUse;
        switch (name) {
          case 'Ed25519':
            // Fall through
          case 'Ed448':
            checkUse = 'sig';
            break;
          case 'X25519':
            // Fall through
          case 'X448':
            checkUse = 'enc';
            break;
        }
        if (keyData.use !== checkUse)
          throw lazyDOMException('Invalid use type', 'DataError');
      }

      validateKeyOps(keyData.key_ops, usagesSet);

      if (keyData.ext !== undefined &&
          keyData.ext === false &&
          extractable === true) {
        throw lazyDOMException('JWK is not extractable', 'DataError');
      }

      if (keyData.alg !== undefined) {
        if (typeof keyData.alg !== 'string')
          throw lazyDOMException('Invalid alg', 'DataError');
        if (
          (name === 'Ed25519' || name === 'Ed448') &&
          keyData.alg !== 'EdDSA'
        ) {
          throw lazyDOMException('Invalid alg', 'DataError');
        }
      }

      verifyAcceptableCfrgKeyUse(
        name,
        isPublic ? 'public' : 'private',
        usagesSet);
      keyObject = createCFRGRawKey(
        name,
        Buffer.from(
          isPublic ? keyData.x : keyData.d,
          'base64'),
        isPublic);
      break;
    }
    case 'raw': {
      verifyAcceptableCfrgKeyUse(name, 'public', usagesSet);
      keyObject = createCFRGRawKey(name, keyData, true);
      if (keyObject === undefined)
        throw lazyDOMException('Unable to import CFRG key', 'OperationError');
      break;
    }
  }

  if (keyObject.asymmetricKeyType !== name.toLowerCase()) {
    throw lazyDOMException('Invalid key type', 'DataError');
  }

  return new InternalCryptoKey(
    keyObject,
    { name },
    keyUsages,
    extractable);
}

function eddsaSignVerify(key, data, { name, context }, signature) {
  emitExperimentalWarning(`The ${name} Web Crypto API algorithm`);
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (key.type !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  if (name === 'Ed448' && context !== undefined) {
    context =
      getArrayBufferOrView(context, 'algorithm.context');
    if (context.byteLength !== 0) {
      throw lazyDOMException(
        'Non zero-length context is not yet supported.', 'NotSupportedError');
    }
  }

  return jobPromise(new SignJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    undefined,
    undefined,
    undefined,
    data,
    undefined,
    undefined,
    undefined,
    undefined,
    signature));
}

module.exports = {
  cfrgExportKey,
  cfrgImportKey,
  cfrgGenerateKey,
  eddsaSignVerify,
};
