'use strict';

const {
  ArrayFrom,
  MathMax,
  MathMin,
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  PromiseResolve,
  RegExpPrototypeTest,
  StringPrototypeToLowerCase,
  Symbol,
  SymbolIterator,
  SymbolToStringTag,
  Uint8Array,
} = primordials;

const {
  createBlob,
  FixedSizeBlobCopyJob,
} = internalBinding('buffer');

const { TextDecoder } = require('internal/encoding');

const {
  JSTransferable,
  kClone,
  kDeserialize,
} = require('internal/worker/js_transferable');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const {
  createDeferredPromise,
  customInspectSymbol: kInspect,
  emitExperimentalWarning,
} = require('internal/util');
const { inspect } = require('internal/util/inspect');

const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_BUFFER_TOO_LARGE,
  }
} = require('internal/errors');

const {
  validateObject,
  isUint32,
} = require('internal/validators');

const kHandle = Symbol('kHandle');
const kType = Symbol('kType');
const kLength = Symbol('kLength');

const disallowedTypeCharacters = /[^\u{0020}-\u{007E}]/u;

let Buffer;

function lazyBuffer() {
  if (Buffer === undefined)
    Buffer = require('buffer').Buffer;
  return Buffer;
}

function isBlob(object) {
  return object?.[kHandle] !== undefined;
}

function getSource(source, encoding) {
  if (isBlob(source))
    return [source.size, source[kHandle]];

  if (isAnyArrayBuffer(source)) {
    source = new Uint8Array(source);
  } else if (!isArrayBufferView(source)) {
    source = lazyBuffer().from(`${source}`, encoding);
  }

  // We copy into a new Uint8Array because the underlying
  // BackingStores are going to be detached and owned by
  // the Blob. We also don't want to have to worry about
  // byte offsets.
  source = new Uint8Array(source);
  return [source.byteLength, source];
}

class InternalBlob extends JSTransferable {
  constructor(handle, length, type = '') {
    super();
    this[kHandle] = handle;
    this[kType] = type;
    this[kLength] = length;
  }
}

class Blob extends JSTransferable {
  constructor(sources = [], options = {}) {
    emitExperimentalWarning('buffer.Blob');
    if (sources === null ||
        typeof sources[SymbolIterator] !== 'function' ||
        typeof sources === 'string') {
      throw new ERR_INVALID_ARG_TYPE('sources', 'Iterable', sources);
    }
    validateObject(options, 'options');
    const { encoding = 'utf8' } = options;
    let { type = '' } = options;

    let length = 0;
    const sources_ = ArrayFrom(sources, (source) => {
      const { 0: len, 1: src } = getSource(source, encoding);
      length += len;
      return src;
    });

    if (!isUint32(length))
      throw new ERR_BUFFER_TOO_LARGE(0xFFFFFFFF);

    super();
    this[kHandle] = createBlob(sources_, length);
    this[kLength] = length;

    type = `${type}`;
    this[kType] = RegExpPrototypeTest(disallowedTypeCharacters, type) ?
      '' : StringPrototypeToLowerCase(type);
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `Blob ${inspect({
      size: this.size,
      type: this.type,
    }, opts)}`;
  }

  [kClone]() {
    const handle = this[kHandle];
    const type = this[kType];
    const length = this[kLength];
    return {
      data: { handle, type, length },
      deserializeInfo: 'internal/blob:InternalBlob'
    };
  }

  [kDeserialize]({ handle, type, length }) {
    this[kHandle] = handle;
    this[kType] = type;
    this[kLength] = length;
  }

  get type() { return this[kType]; }

  get size() { return this[kLength]; }

  slice(start = 0, end = this[kLength], contentType = '') {
    if (start < 0) {
      start = MathMax(this[kLength] + start, 0);
    } else {
      start = MathMin(start, this[kLength]);
    }
    start |= 0;

    if (end < 0) {
      end = MathMax(this[kLength] + end, 0);
    } else {
      end = MathMin(end, this[kLength]);
    }
    end |= 0;

    contentType = `${contentType}`;
    if (RegExpPrototypeTest(disallowedTypeCharacters, contentType)) {
      contentType = '';
    } else {
      contentType = StringPrototypeToLowerCase(contentType);
    }

    const span = MathMax(end - start, 0);

    return new InternalBlob(
      this[kHandle].slice(start, start + span), span, contentType);
  }

  async arrayBuffer() {
    const job = new FixedSizeBlobCopyJob(this[kHandle]);

    const ret = job.run();
    if (ret !== undefined)
      return PromiseResolve(ret);

    const {
      promise,
      resolve,
      reject
    } = createDeferredPromise();
    job.ondone = (err, ab) => {
      if (err !== undefined)
        return reject(new AbortError());
      resolve(ab);
    };

    return promise;
  }

  async text() {
    const dec = new TextDecoder();
    return dec.decode(await this.arrayBuffer());
  }
}

ObjectDefineProperty(Blob.prototype, SymbolToStringTag, {
  configurable: true,
  value: 'Blob',
});

InternalBlob.prototype.constructor = Blob;
ObjectSetPrototypeOf(
  InternalBlob.prototype,
  Blob.prototype);

module.exports = {
  Blob,
  InternalBlob,
  isBlob,
};
