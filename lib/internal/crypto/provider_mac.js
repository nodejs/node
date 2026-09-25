'use strict';

const {
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
  StringPrototypeIncludes,
  StringPrototypeToLowerCase,
  Symbol,
} = primordials;

const {
  Mac: _Mac,
} = internalBinding('crypto');

const {
  getCachedMacId,
  getMacCache,
  kHandle,
} = require('internal/crypto/util');

const {
  getKeyObjectHandle,
  getKeyObjectType,
  isKeyObject,
} = require('internal/crypto/keys');

const {
  normalizeEncoding,
} = require('internal/util');

const {
  codes: {
    ERR_CRYPTO_MAC_FINALIZED,
    ERR_CRYPTO_MAC_UPDATE_FAILED,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');

const {
  validateEncoding,
  validateObject,
  validateString,
  validateUint32,
} = require('internal/validators');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const LazyTransform = require('internal/streams/lazy_transform');

const kState = Symbol('kState');
const kFinalized = Symbol('kFinalized');

function validateName(value, name) {
  validateString(value, name);
  if (value.length === 0 || StringPrototypeIncludes(value, '\0')) {
    throw new ERR_INVALID_ARG_VALUE(
      name, value, 'must be non-empty and contain no NUL bytes');
  }
  return value;
}

function normalizeBytes(value, name) {
  if (!isArrayBufferView(value) && !isAnyArrayBuffer(value)) {
    throw new ERR_INVALID_ARG_TYPE(
      name, ['ArrayBuffer', 'Buffer', 'TypedArray', 'DataView'], value);
  }
  return value;
}

function createHandle(algorithm, key, options) {
  const name = validateName(algorithm, 'algorithm');
  let digest;
  let cipher;
  let iv;
  let customization;
  let salt;
  let outputLength;
  let hasExtendedOptions = false;

  if (options !== undefined) {
    validateObject(options, 'options');

    const digestOption = options.digest;
    if (digestOption !== undefined) {
      digest = validateName(digestOption, 'options.digest');
    }
    const cipherOption = options.cipher;
    if (cipherOption !== undefined) {
      cipher = validateName(cipherOption, 'options.cipher');
      hasExtendedOptions = true;
    }
    const ivOption = options.iv;
    if (ivOption !== undefined) {
      iv = normalizeBytes(ivOption, 'options.iv');
      hasExtendedOptions = true;
    }
    const customizationOption = options.customization;
    if (customizationOption !== undefined) {
      customization = normalizeBytes(
        customizationOption, 'options.customization');
      hasExtendedOptions = true;
    }
    const saltOption = options.salt;
    if (saltOption !== undefined) {
      salt = normalizeBytes(saltOption, 'options.salt');
      hasExtendedOptions = true;
    }
    const outputLengthOption = options.outputLength;
    if (outputLengthOption !== undefined) {
      outputLength = outputLengthOption;
      validateUint32(outputLength, 'options.outputLength');
      outputLength += 0;
      hasExtendedOptions = true;
    }
  }

  key = normalizeKey(key);
  const id = getCachedMacId(name);
  const cache = getMacCache();
  if (!hasExtendedOptions) {
    return new _Mac(name, id, cache, key, digest);
  }
  return new _Mac(
    name,
    id,
    cache,
    key,
    digest,
    cipher,
    iv,
    customization,
    salt,
    outputLength,
  );
}

function normalizeKey(key) {
  if (isKeyObject(key)) {
    if (getKeyObjectType(key) !== 'secret') {
      throw new ERR_INVALID_ARG_TYPE(
        'key', ['ArrayBuffer', 'Buffer', 'TypedArray', 'DataView', 'KeyObject'], key);
    }
    return getKeyObjectHandle(key);
  }
  if (!isArrayBufferView(key) && !isAnyArrayBuffer(key)) {
    throw new ERR_INVALID_ARG_TYPE(
      'key', ['ArrayBuffer', 'Buffer', 'TypedArray', 'DataView', 'KeyObject'], key);
  }
  return key;
}

function normalizeOutputEncoding(outputEncoding) {
  if (outputEncoding === undefined) return 'buffer';
  validateString(outputEncoding, 'outputEncoding');
  if (StringPrototypeToLowerCase(outputEncoding) === 'buffer') return 'buffer';
  const normalized = normalizeEncoding(outputEncoding);
  if (normalized === undefined) {
    throw new ERR_INVALID_ARG_VALUE('outputEncoding', outputEncoding);
  }
  return normalized;
}

function normalizeInputEncoding(data, inputEncoding) {
  if (inputEncoding === undefined) return undefined;
  validateString(inputEncoding, 'inputEncoding');
  const normalized = normalizeEncoding(inputEncoding);
  if (normalized === undefined) {
    throw new ERR_INVALID_ARG_VALUE('inputEncoding', inputEncoding);
  }
  validateEncoding(data, normalized);
  return normalized;
}

function updateHandle(mac, data, encoding) {
  const state = mac[kState];
  let updated;
  try {
    updated = mac[kHandle].update(data, encoding);
  } catch (error) {
    state[kFinalized] = true;
    throw error;
  }
  if (!updated) {
    state[kFinalized] = true;
    throw new ERR_CRYPTO_MAC_UPDATE_FAILED();
  }
}

function finalizeHandle(mac, outputEncoding) {
  const state = mac[kState];
  state[kFinalized] = true;
  return mac[kHandle].final(outputEncoding);
}

function Mac(algorithm, key, options) {
  if (!new.target) return new Mac(algorithm, key, options);
  this[kHandle] = createHandle(algorithm, key, options);
  this[kState] = {
    [kFinalized]: false,
  };
  FunctionPrototypeCall(LazyTransform, this, options);
}

ObjectSetPrototypeOf(Mac.prototype, LazyTransform.prototype);
ObjectSetPrototypeOf(Mac, LazyTransform);

Mac.prototype._transform = function _transform(chunk, encoding, callback) {
  if (this[kState][kFinalized]) {
    callback(new ERR_CRYPTO_MAC_FINALIZED());
    return;
  }
  try {
    updateHandle(this, chunk, encoding);
  } catch (error) {
    callback(error);
    return;
  }
  callback();
};

Mac.prototype._flush = function _flush(callback) {
  if (this[kState][kFinalized]) {
    callback(new ERR_CRYPTO_MAC_FINALIZED());
    return;
  }
  try {
    const result = finalizeHandle(this);
    if (result.length !== 0) this.push(result);
  } catch (error) {
    callback(error);
    return;
  }
  callback();
};

Mac.prototype.update = function update(data, encoding) {
  if (this[kState][kFinalized]) throw new ERR_CRYPTO_MAC_FINALIZED();
  if (typeof data === 'string') {
    encoding = normalizeInputEncoding(data, encoding);
  } else if (!isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE(
      'data', ['string', 'Buffer', 'TypedArray', 'DataView'], data);
  }
  updateHandle(this, data, encoding);
  return this;
};

Mac.prototype.final = function final(outputEncoding) {
  if (this[kState][kFinalized]) throw new ERR_CRYPTO_MAC_FINALIZED();
  if (outputEncoding === undefined) return finalizeHandle(this);
  outputEncoding = normalizeOutputEncoding(outputEncoding);
  if (outputEncoding === 'buffer') return finalizeHandle(this);
  return finalizeHandle(this, outputEncoding);
};

module.exports = {
  Mac,
};
