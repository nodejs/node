'use strict';

const {
  ArrayFrom,
  MathMax,
  MathMin,
  ObjectDefineProperty,
  PromiseResolve,
  PromiseReject,
  PromisePrototypeFinally,
  ReflectConstruct,
  RegExpPrototypeTest,
  StringPrototypeToLowerCase,
  Symbol,
  SymbolIterator,
  SymbolToStringTag,
  Uint8Array,
} = primordials;

const {
  createBlob: _createBlob,
  FixedSizeBlobCopyJob,
} = internalBinding('buffer');

const { TextDecoder } = require('internal/encoding');

const {
  makeTransferable,
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
    ERR_INVALID_THIS,
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
const kArrayBufferPromise = Symbol('kArrayBufferPromise');

const disallowedTypeCharacters = /[^\u{0020}-\u{007E}]/u;

let Buffer;
let ReadableStream;

function lazyBuffer() {
  if (Buffer === undefined)
    Buffer = require('buffer').Buffer;
  return Buffer;
}

function lazyReadableStream(options) {
  if (ReadableStream === undefined) {
    ReadableStream =
      require('internal/webstreams/readablestream').ReadableStream;
  }
  return new ReadableStream(options);
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

class Blob {
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

    this[kHandle] = _createBlob(sources_, length);
    this[kLength] = length;

    type = `${type}`;
    this[kType] = RegExpPrototypeTest(disallowedTypeCharacters, type) ?
      '' : StringPrototypeToLowerCase(type);

    // eslint-disable-next-line no-constructor-return
    return makeTransferable(this);
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
      deserializeInfo: 'internal/blob:ClonedBlob'
    };
  }

  [kDeserialize]({ handle, type, length }) {
    this[kHandle] = handle;
    this[kType] = type;
    this[kLength] = length;
  }

  /**
   * @readonly
   * @type {string}
   */
  get type() {
    if (!isBlob(this))
      throw new ERR_INVALID_THIS('Blob');
    return this[kType];
  }

  /**
   * @readonly
   * @type {number}
   */
  get size() {
    if (!isBlob(this))
      throw new ERR_INVALID_THIS('Blob');
    return this[kLength];
  }

  /**
   * @param {number} [start]
   * @param {number} [end]
   * @param {string} [contentType]
   * @returns {Blob}
   */
  slice(start = 0, end = this[kLength], contentType = '') {
    if (!isBlob(this))
      throw new ERR_INVALID_THIS('Blob');
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

    return createBlob(
      this[kHandle].slice(start, start + span),
      span,
      contentType);
  }

  /**
   * @returns {Promise<ArrayBuffer>}
   */
  arrayBuffer() {
    if (!isBlob(this))
      return PromiseReject(new ERR_INVALID_THIS('Blob'));

    // If there's already a promise in flight for the content,
    // reuse it, but only once. After the cached promise resolves
    // it will be cleared, allowing it to be garbage collected
    // as soon as possible.
    if (this[kArrayBufferPromise])
      return this[kArrayBufferPromise];

    const job = new FixedSizeBlobCopyJob(this[kHandle]);

    const ret = job.run();

    // If the job returns a value immediately, the ArrayBuffer
    // was generated synchronously and should just be returned
    // directly.
    if (ret !== undefined)
      return PromiseResolve(ret);

    const {
      promise,
      resolve,
      reject,
    } = createDeferredPromise();

    job.ondone = (err, ab) => {
      if (err !== undefined)
        return reject(new AbortError());
      resolve(ab);
    };
    this[kArrayBufferPromise] =
    PromisePrototypeFinally(
      promise,
      () => this[kArrayBufferPromise] = undefined);

    return this[kArrayBufferPromise];
  }

  /**
   *
   * @returns {Promise<string>}
   */
  async text() {
    if (!isBlob(this))
      throw new ERR_INVALID_THIS('Blob');

    const dec = new TextDecoder();
    return dec.decode(await this.arrayBuffer());
  }

  /**
   * @returns {ReadableStream}
   */
  stream() {
    if (!isBlob(this))
      throw new ERR_INVALID_THIS('Blob');

    const self = this;
    return new lazyReadableStream({
      async start(controller) {
        const ab = await self.arrayBuffer();
        controller.enqueue(new Uint8Array(ab));
        controller.close();
      }
    });
  }
}

function ClonedBlob() {
  return makeTransferable(ReflectConstruct(function() {}, [], Blob));
}
ClonedBlob.prototype[kDeserialize] = () => {};

function createBlob(handle, length, type = '') {
  return makeTransferable(ReflectConstruct(function() {
    this[kHandle] = handle;
    this[kType] = type;
    this[kLength] = length;
  }, [], Blob));
}

ObjectDefineProperty(Blob.prototype, SymbolToStringTag, {
  configurable: true,
  value: 'Blob',
});

module.exports = {
  Blob,
  ClonedBlob,
  createBlob,
  isBlob,
};
