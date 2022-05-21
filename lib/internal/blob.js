'use strict';

const {
  ArrayFrom,
  MathMax,
  MathMin,
  ObjectDefineProperty,
  PromiseResolve,
  PromiseReject,
  SafePromisePrototypeFinally,
  ReflectConstruct,
  RegExpPrototypeSymbolReplace,
  RegExpPrototypeTest,
  StringPrototypeToLowerCase,
  StringPrototypeSplit,
  Symbol,
  SymbolIterator,
  SymbolToStringTag,
  Uint8Array,
} = primordials;

const {
  createBlob: _createBlob,
  FixedSizeBlobCopyJob,
  getDataObject,
} = internalBinding('blob');

const {
  TextDecoder,
  TextEncoder,
} = require('internal/encoding');

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
  kEmptyObject,
} = require('internal/util');
const { inspect } = require('internal/util/inspect');

const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
    ERR_BUFFER_TOO_LARGE,
  }
} = require('internal/errors');

const {
  validateObject,
  isUint32,
} = require('internal/validators');

const kHandle = Symbol('kHandle');
const kState = Symbol('kState');
const kIndex = Symbol('kIndex');
const kType = Symbol('kType');
const kLength = Symbol('kLength');
const kArrayBufferPromise = Symbol('kArrayBufferPromise');

const kMaxChunkSize = 65536;

const disallowedTypeCharacters = /[^\u{0020}-\u{007E}]/u;

let ReadableStream;
let URL;

const enc = new TextEncoder();

// Yes, lazy loading is annoying but because of circular
// references between the url, internal/blob, and buffer
// modules, lazy loading here makes sure that things work.

function lazyURL(id) {
  URL ??= require('internal/url').URL;
  return new URL(id);
}

function lazyReadableStream(options) {
  // eslint-disable-next-line no-global-assign
  ReadableStream ??=
    require('internal/webstreams/readablestream').ReadableStream;
  return new ReadableStream(options);
}

const { EOL } = require('internal/constants');

function isBlob(object) {
  return object?.[kHandle] !== undefined;
}

function getSource(source, endings) {
  if (isBlob(source))
    return [source.size, source[kHandle]];

  if (isAnyArrayBuffer(source)) {
    source = new Uint8Array(source);
  } else if (!isArrayBufferView(source)) {
    source = `${source}`;
    if (endings === 'native')
      source = RegExpPrototypeSymbolReplace(/\n|\r\n/g, source, EOL);
    source = enc.encode(source);
  }

  // We copy into a new Uint8Array because the underlying
  // BackingStores are going to be detached and owned by
  // the Blob.
  const { buffer, byteOffset, byteLength } = source;
  const slice = buffer.slice(byteOffset, byteOffset + byteLength);
  return [byteLength, new Uint8Array(slice)];
}

class Blob {
  /**
   * @typedef {string|ArrayBuffer|ArrayBufferView|Blob} SourcePart
   */

  /**
   * @param {SourcePart[]} [sources]
   * @param {{
   *   endings? : string,
   *   type? : string,
   * }} [options]
   * @constructs {Blob}
   */
  constructor(sources = [], options = kEmptyObject) {
    if (sources === null ||
        typeof sources[SymbolIterator] !== 'function' ||
        typeof sources === 'string') {
      throw new ERR_INVALID_ARG_TYPE('sources', 'a sequence', sources);
    }
    validateObject(options, 'options');
    let {
      type = '',
      endings = 'transparent',
    } = options;

    endings = `${endings}`;
    if (endings !== 'transparent' && endings !== 'native')
      throw new ERR_INVALID_ARG_VALUE('options.endings', endings);

    let length = 0;
    const sources_ = ArrayFrom(sources, (source) => {
      const { 0: len, 1: src } = getSource(source, endings);
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
    // reuse it, but only while it's in flight. After the cached
    // promise resolves it will be cleared, allowing it to be
    // garbage collected as soon as possible.
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
        return reject(new AbortError(undefined, { cause: err }));
      resolve(ab);
    };
    this[kArrayBufferPromise] =
    SafePromisePrototypeFinally(
      promise,
      () => this[kArrayBufferPromise] = undefined);

    return this[kArrayBufferPromise];
  }

  /**
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
      async start() {
        this[kState] = await self.arrayBuffer();
        this[kIndex] = 0;
      },

      pull(controller) {
        if (this[kState].byteLength - this[kIndex] <= kMaxChunkSize) {
          controller.enqueue(new Uint8Array(this[kState], this[kIndex]));
          controller.close();
          this[kState] = undefined;
        } else {
          controller.enqueue(new Uint8Array(this[kState], this[kIndex], kMaxChunkSize));
          this[kIndex] += kMaxChunkSize;
        }
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
  __proto__: null,
  configurable: true,
  value: 'Blob',
});

function resolveObjectURL(url) {
  url = `${url}`;
  try {
    const parsed = new lazyURL(url);

    const split = StringPrototypeSplit(parsed.pathname, ':');

    if (split.length !== 2)
      return;

    const {
      0: base,
      1: id,
    } = split;

    if (base !== 'nodedata')
      return;

    const ret = getDataObject(id);

    if (ret === undefined)
      return;

    const {
      0: handle,
      1: length,
      2: type,
    } = ret;

    if (handle !== undefined)
      return createBlob(handle, length, type);
  } catch {
    // If there's an error, it's ignored and nothing is returned
  }
}

module.exports = {
  Blob,
  ClonedBlob,
  createBlob,
  isBlob,
  kHandle,
  resolveObjectURL,
};
