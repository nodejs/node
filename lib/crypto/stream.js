'use strict';

const {
  ArrayPrototypeIncludes,
} = primordials;

const {
  DigestSink,
  HmacSink,
} = require('internal/crypto/hash');

const {
  RsaSignSink,
} = require('internal/crypto/rsa');

const {
  EcSignSink,
} = require('internal/crypto/ec');

const {
  DsaSignSink,
} = require('internal/crypto/dsa');

const {
  kHandle,
  normalizeAlgorithm,
  normalizeHashName,
  getArrayBufferOrView,
} = require('internal/crypto/util');

const {
  isCryptoKey,
  kKeyObject,
} = require('internal/crypto/keys');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const {
  WritableStream,
} = require('internal/webstreams/writablestream');

const {
  lazyDOMException,
} = require('internal/util');

class DigestStream extends WritableStream {
  constructor(algorithm, queuingStrategy) {
    const sink = new DigestSink(algorithm);
    super(sink, queuingStrategy);
    this[kHandle] = sink;
  }

  /**
   * @readonly
   * @type {Promise<ArrayBuffer>}
   */
  get digest() {
    return this[kHandle].digest;
  }
}

class SignStream extends WritableStream {
  constructor(algorithm, key, queuingStrategy) {
    algorithm = normalizeAlgorithm(algorithm);
    if (!isCryptoKey(key))
      throw new ERR_INVALID_ARG_TYPE('key', 'CryptoKey', key);

    if (!ArrayPrototypeIncludes(key.usages, 'sign') ||
        algorithm.name !== key.algorithm.name) {
      throw lazyDOMException(
        'Unable to use this key to sign',
        'InvalidAccessError');
    }

    let sink;
    switch (algorithm.name) {
      case 'RSA-PSS':
      case 'RSASSA-PKCS1-v1_5':
        sink = new RsaSignSink(algorithm, key);
        break;
      case 'NODE-ED25519':
      case 'NODE-ED448':
      case 'ECDSA':
        sink = new EcSignSink(algorithm, key);
        break;
      case 'NODE-DSA':
        sink = new DsaSignSink(algorithm, key);
        break;
      case 'HMAC': {
        sink = new HmacSink(
          normalizeHashName(algorithm.hash.name),
          key[kKeyObject]);
        break;
      }
      default:
        throw lazyDOMException('Unrecognized named.', 'NotSupportedError');
    }

    super(sink, queuingStrategy);
    this[kHandle] = sink;
  }

  /**
   * @readonly
   * @type {Promise<ArrayBuffer>}
   */
  get signature() {
    return this[kHandle].signature;
  }
}

class VerifyStream extends WritableStream {
  constructor(algorithm, key, signature, queuingStrategy) {
    algorithm = normalizeAlgorithm(algorithm);
    if (!isCryptoKey(key))
      throw new ERR_INVALID_ARG_TYPE('key', 'CryptoKey', key);

    if (!ArrayPrototypeIncludes(key.usages, 'verify') ||
        algorithm.name !== key.algorithm.name) {
      throw lazyDOMException(
        'Unable to use this key to sign',
        'InvalidAccessError');
    }

    signature = getArrayBufferOrView(signature, 'signature');

    let sink;
    switch (algorithm.name) {
      case 'RSA-PSS':
      case 'RSASSA-PKCS1-v1_5':
        sink = new RsaSignSink(algorithm, key, signature);
        break;
      case 'NODE-ED25519':
      case 'NODE-ED448':
      case 'ECDSA':
        sink = new EcSignSink(algorithm, key, signature);
        break;
      case 'NODE-DSA':
        sink = new DsaSignSink(algorithm, key, signature);
        break;
      case 'HMAC': {
        sink = new HmacSink(
          normalizeHashName(algorithm.hash.name),
          key[kKeyObject],
          signature);
        break;
      }
      default:
        throw lazyDOMException('Unrecognized named.', 'NotSupportedError');
    }

    super(sink, queuingStrategy);
    this[kHandle] = sink;
  }

  /**
   * @readonly
   * @type {Promise<bool>}
   */
  get verified() {
    return this[kHandle].signature;
  }
}

module.exports = {
  DigestStream,
  SignStream,
  VerifyStream,
};
