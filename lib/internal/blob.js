'use strict';

const {
  ArrayBufferPrototypeGetByteLength,
  ArrayBufferPrototypeGetDetached,
  ArrayPrototypePush,
  DataViewPrototypeGetBuffer,
  DataViewPrototypeGetByteLength,
  DataViewPrototypeGetByteOffset,
  MathMax,
  MathMin,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseReject,
  PromiseWithResolvers,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolReplace,
  SafeWeakSet,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  Symbol,
  SymbolToStringTag,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  Uint8Array,
} = primordials;

const {
  createBlob: _createBlob,
  createBlobFromFilePath: _createBlobFromFilePath,
  concat,
  getDataObject,
} = internalBinding('blob');
const {
  kMaxLength,
} = internalBinding('buffer');

const {
  getUtf8Decoder,
  TextEncoder,
} = require('internal/encoding');
const { URL } = require('internal/url');

const {
  markTransferMode,
  kClone,
  kDeserialize,
} = require('internal/worker/js_transferable');

const {
  isAnyArrayBuffer,
  isArrayBuffer,
  isArrayBufferView,
  isDataView,
} = require('internal/util/types');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
  kEnumerableProperty,
  lazyDOMException,
} = require('internal/util');
const { inspect } = require('internal/util/inspect');
const {
  converters,
  createDictionaryConverter,
  createEnumConverter,
  createSequenceConverter,
} = require('internal/webidl');

const {
  codes: {
    ERR_BUFFER_TOO_LARGE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  setImmediate,
} = require('timers');

const { queueMicrotask } = require('internal/process/task_queues');

const kHandle = Symbol('kHandle');
const kType = Symbol('kType');
const kLength = Symbol('kLength');
const kNotCloneable = Symbol('kNotCloneable');
const kAlreadyConverted = Symbol('kAlreadyConverted');
const blobBrand = new SafeWeakSet();

const disallowedTypeCharacters = /[^\u{0020}-\u{007E}]/u;

let ReadableStream;
let TextDecoderStream;

const enc = new TextEncoder();

// Yes, lazy loading is annoying but because of circular
// references between the url, internal/blob, and buffer
// modules, lazy loading here makes sure that things work.

function lazyReadableStream(options) {
  // eslint-disable-next-line no-global-assign
  ReadableStream ??=
    require('internal/webstreams/readablestream').ReadableStream;
  return new ReadableStream(options);
}

function lazyTextDecoderStream() {
  // eslint-disable-next-line no-global-assign
  TextDecoderStream ??=
    require('internal/webstreams/encoding').TextDecoderStream;
  return new TextDecoderStream();
}

const { EOL } = require('internal/constants');

function isBlob(object) {
  return blobBrand.has(object);
}

function getSource(source, endings) {
  if (isBlob(source))
    return [source[kLength], source[kHandle]];

  if (typeof source === 'string') {
    if (endings === 'native')
      source = RegExpPrototypeSymbolReplace(/\r\n|\r|\n/g, source, EOL);
    source = enc.encode(source);
  }

  // We copy into a new Uint8Array because the underlying
  // BackingStores are going to be detached and owned by
  // the Blob.
  const isBuffer = isArrayBuffer(source);
  const isDataViewSource = !isBuffer && isDataView(source);
  const buffer = isBuffer ? source :
    isDataViewSource ? DataViewPrototypeGetBuffer(source) : TypedArrayPrototypeGetBuffer(source);
  // Check before reading byteOffset and byteLength, whose DataView getters
  // throw when the backing buffer is detached.
  if (ArrayBufferPrototypeGetDetached(buffer))
    return [0, new Uint8Array()];
  const byteOffset = isBuffer ? 0 :
    isDataViewSource ? DataViewPrototypeGetByteOffset(source) : TypedArrayPrototypeGetByteOffset(source);
  const byteLength = isBuffer ? ArrayBufferPrototypeGetByteLength(source) :
    isDataViewSource ? DataViewPrototypeGetByteLength(source) : TypedArrayPrototypeGetByteLength(source);
  const copy = new Uint8Array(new Uint8Array(buffer, byteOffset, byteLength));
  return [byteLength, copy];
}

const blobPartsConverter = createSequenceConverter((source, opts = kEmptyObject) => {
  if (isBlob(source)) {
    return source;
  }
  // Match browsers for bare SharedArrayBuffer pending a resolution of
  // https://github.com/whatwg/webidl/issues/1491.
  if (isAnyArrayBuffer(source) || isArrayBufferView(source)) {
    return converters.BufferSource(source, opts);
  }
  return converters.USVString(source, opts);
});

const blobPropertyBag = [
  {
    key: 'endings',
    converter: createEnumConverter('EndingType', ['transparent', 'native']),
    defaultValue: () => 'transparent',
  },
  {
    key: 'type',
    converter: converters.DOMString,
    defaultValue: () => '',
  },
];

const blobPropertyBagConverter = createDictionaryConverter('BlobPropertyBag', blobPropertyBag);

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
   * @constructs Blob
   */
  constructor(sources = [], options, alreadyConverted) {
    markTransferMode(this, true, false);

    // File converts all of its arguments before entering the Blob constructor.
    if (alreadyConverted !== kAlreadyConverted) {
      sources = blobPartsConverter(sources, {
        __proto__: null,
        context: 'sources',
      });
      options = blobPropertyBagConverter(options, {
        __proto__: null,
        context: 'options',
      });
    }
    const { endings, type } = options;

    let length = 0;
    for (let i = 0; i < sources.length; ++i) {
      const { 0: len, 1: src } = getSource(sources[i], endings);
      length += len;
      sources[i] = src;
    }

    if (length > kMaxLength)
      throw new ERR_BUFFER_TOO_LARGE(kMaxLength);

    this[kHandle] = _createBlob(sources, length);
    this[kLength] = length;

    this[kType] = RegExpPrototypeExec(disallowedTypeCharacters, type) !== null ?
      '' : StringPrototypeToLowerCase(type);
    blobBrand.add(this);
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `Blob ${inspect({
      size: this.size,
      type: this.type,
    }, opts)}`;
  }

  [kClone]() {
    if (this[kNotCloneable]) {
      // We do not currently allow file-backed Blobs to be cloned or passed across
      // worker threads.
      throw new ERR_INVALID_STATE.TypeError('File-backed Blobs are not cloneable');
    }
    const handle = this[kHandle];
    const type = this[kType];
    const length = this[kLength];
    return {
      data: { handle, type, length },
      deserializeInfo: 'internal/blob:Blob',
    };
  }

  [kDeserialize]({ handle, type, length }) {
    this[kHandle] = handle;
    this[kType] = type;
    this[kLength] = length;
    blobBrand.add(this);
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

    start = converters['long long'](
      start,
      { __proto__: null, context: 'start', clamp: true },
    );
    end = converters['long long'](
      end,
      { __proto__: null, context: 'end', clamp: true },
    );

    if (start < 0) {
      start = MathMax(this[kLength] + start, 0);
    } else {
      start = MathMin(start, this[kLength]);
    }

    if (end < 0) {
      end = MathMax(this[kLength] + end, 0);
    } else {
      end = MathMin(end, this[kLength]);
    }

    contentType = `${contentType}`;
    if (RegExpPrototypeExec(disallowedTypeCharacters, contentType) !== null) {
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

    return arrayBuffer(this);
  }

  /**
   * @returns {Promise<string>}
   */
  text() {
    if (!isBlob(this))
      return PromiseReject(new ERR_INVALID_THIS('Blob'));

    const dec = getUtf8Decoder();

    return PromisePrototypeThen(
      arrayBuffer(this),
      (buffer) => dec.decode(buffer));
  }

  /**
   * @returns {Promise<Uint8Array>}
   */
  bytes() {
    if (!isBlob(this))
      return PromiseReject(new ERR_INVALID_THIS('Blob'));

    return PromisePrototypeThen(
      arrayBuffer(this),
      (buffer) => new Uint8Array(buffer));
  }

  /**
   * @returns {ReadableStream}
   */
  stream() {
    if (!isBlob(this))
      throw new ERR_INVALID_THIS('Blob');
    return createBlobReaderStream(this[kHandle].getReader());
  }

  /**
   * @returns {ReadableStream}
   */
  textStream() {
    if (!isBlob(this))
      throw new ERR_INVALID_THIS('Blob');
    const stream = createBlobReaderStream(this[kHandle].getReader());
    const decoder = lazyTextDecoderStream();
    return stream.pipeThrough(decoder);
  }
}

function TransferableBlob(handle, length, type = '') {
  ObjectSetPrototypeOf(this, Blob.prototype);
  markTransferMode(this, true, false);
  this[kHandle] = handle;
  this[kType] = type;
  this[kLength] = length;
  blobBrand.add(this);
}

ObjectSetPrototypeOf(TransferableBlob.prototype, Blob.prototype);
ObjectSetPrototypeOf(TransferableBlob, Blob);

function createBlob(handle, length, type = '') {
  const transferredBlob = new TransferableBlob(handle, length, type);

  // Fix issues like: https://github.com/nodejs/node/pull/49730#discussion_r1331720053
  transferredBlob.constructor = Blob;

  return transferredBlob;
}

ObjectDefineProperty(Blob.prototype, SymbolToStringTag, {
  __proto__: null,
  configurable: true,
  value: 'Blob',
});

ObjectDefineProperties(Blob.prototype, {
  size: kEnumerableProperty,
  type: kEnumerableProperty,
  slice: kEnumerableProperty,
  stream: kEnumerableProperty,
  textStream: kEnumerableProperty,
  text: kEnumerableProperty,
  arrayBuffer: kEnumerableProperty,
  bytes: kEnumerableProperty,
});

function resolveObjectURL(url) {
  url = `${url}`;
  try {
    const parsed = new URL(url);

    const split = StringPrototypeSplit(parsed.pathname, ':', 3);

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

// TODO(@jasnell): Now that the File class exists, we might consider having
// this return a `File` instead of a `Blob`.
function createBlobFromFilePath(path, options) {
  const maybeBlob = _createBlobFromFilePath(path);
  if (maybeBlob === undefined) {
    return lazyDOMException('The blob could not be read', 'NotReadableError');
  }
  const { 0: blob, 1: length } = maybeBlob;
  const res = createBlob(blob, length, options?.type);
  res[kNotCloneable] = true;
  return res;
}

function arrayBuffer(blob) {
  const { promise, resolve, reject } = PromiseWithResolvers();
  const reader = blob[kHandle].getReader();
  const buffers = [];
  const readNext = () => {
    reader.pull((status, buffer) => {
      if (status === 0) {
        // EOS, concat & resolve
        // buffer should be undefined here
        resolve(concat(buffers));
        return;
      } else if (status < 0) {
        // The read could fail for many different reasons when reading
        // from a non-memory resident blob part (e.g. file-backed blob).
        // The error details the system error code.
        const error = lazyDOMException('The blob could not be read', 'NotReadableError');
        reject(error);
        return;
      }
      if (buffer !== undefined)
        buffers.push(buffer);
      queueMicrotask(() => readNext());
    });
  };
  readNext();
  return promise;
}

/**
 * Read a blob's data synchronously. This is only possible when every part
 * of the blob is memory-resident, in which case the reader's pull callbacks
 * are invoked synchronously; otherwise (e.g. for file-backed blobs)
 * undefined is returned.
 * @param {Blob} blob
 * @returns {ArrayBuffer|undefined}
 */
function getBlobDataSync(blob) {
  const reader = blob[kHandle].getReader();
  const buffers = [];
  let result;
  let ended = false;
  while (!ended) {
    let sync = false;
    reader.pull((status, buffer) => {
      sync = true;
      if (status === 0) {
        // EOS; buffer should be undefined here.
        result = concat(buffers);
        ended = true;
        return;
      } else if (status < 0) {
        ended = true;
        return;
      }
      if (buffer !== undefined)
        ArrayPrototypePush(buffers, buffer);
    });
    if (!sync) {
      // The data is not available synchronously.
      break;
    }
  }
  return result;
}

function createBlobReaderStream(reader) {
  return new lazyReadableStream({
    type: 'bytes',
    start(c) {
      // There really should only be one read at a time so using an
      // array here is purely defensive.
      this.pendingPulls = [];
      // Lazily register a wakeup callback that the C++ side can invoke
      // when new data is available after a STATUS_BLOCK.
      let immediate;
      this.wakeup = () => {
        if (this.pendingPulls.length > 0) {
          immediate ??= setImmediate(() => {
            immediate = undefined;
            this.readNext(c);
          });
        }
      };
    },
    pull(c) {
      const { promise, resolve, reject } = PromiseWithResolvers();
      if (this.pendingPulls.length === 0) {
        reader.setWakeup(this.wakeup);
      }
      this.pendingPulls.push({ resolve, reject });
      this.readNext(c);
      return promise;
    },
    clearWakeupIfIdle() {
      if (this.pendingPulls.length === 0) {
        reader.setWakeup(undefined);
      }
    },
    readNext(c) {
      reader.pull((status, buffer) => {
        // If pendingPulls is empty here, the stream had to have
        // been canceled, and we don't really care about the result.
        // We can simply exit.
        if (this.pendingPulls.length === 0) {
          reader.setWakeup(undefined);
          return;
        }
        if (status === 0) {
          // EOS
          reader.setWakeup(undefined);
          c.close();
          // This is to signal the end for byob readers
          // see https://streams.spec.whatwg.org/#example-rbs-pull
          c.byobRequest?.respond(0);
          const pending = this.pendingPulls.shift();
          pending.resolve();
          return;
        } else if (status < 0) {
          // The read could fail for many different reasons when reading
          // from a non-memory resident blob part (e.g. file-backed blob).
          // The error details the system error code.
          reader.setWakeup(undefined);
          const error =
            lazyDOMException('The blob could not be read',
                             'NotReadableError');
          const pending = this.pendingPulls.shift();
          c.error(error);
          pending.reject(error);
          return;
        } else if (status === 2) {
          // STATUS_BLOCK: No data available yet. The wakeup callback
          // registered in pull() will re-invoke readNext when data
          // arrives.
          return;
        }
        // ReadableByteStreamController.enqueue errors if we submit a
        // 0-length buffer. We need to check for that here.
        if (buffer !== undefined && buffer.byteLength !== 0) {
          c.enqueue(new Uint8Array(buffer));
        }
        // We keep reading until we either reach EOS, some error, or
        // we hit the flow rate of the stream (c.desiredSize).
        // We use setImmediate here because we have to allow the event
        // loop to turn in order to process any pending i/o. Using
        // queueMicrotask won't allow the event loop to turn.
        setImmediate(() => {
          if (c.desiredSize < 0) {
            // A manual backpressure check.
            if (this.pendingPulls.length !== 0) {
              const pending = this.pendingPulls.shift();
              pending.resolve();
              this.clearWakeupIfIdle();
            }
            return;
          }
          this.readNext(c);
        });
      });
    },
    cancel(reason) {
      reader.setWakeup(undefined);
      // Reject any currently pending pulls here.
      for (const pending of this.pendingPulls) {
        pending.reject(reason);
      }
      this.pendingPulls = [];
    },
  // We set the highWaterMark to 0 because we do not want the stream to
  // start reading immediately on creation. We want it to wait until read
  // is called.
  }, { highWaterMark: 0 });
}

// Upper bound on the number of chunks collected in a single batch. This is
// only a cap on the length of the yielded array -- the primary limit is the
// byte budget below, since under a byte-budget backpressure model the size of
// a batch is what matters, not how many pieces it arrives in.
const kMaxBatchChunks = 16;

// Default number of bytes to collect in a single batch. Entries in the
// DataQueue can each be as large as the peer's flow control window, so a
// purely count-based limit could produce enormous batches (16 entries of
// 1 MB each).
//
// This matters for more than just the size of the yielded array. Consumers
// like QUIC return flow control credit from the reader's pull path -- once
// per pull, not once per batch -- so every pull this loop performs invites
// the peer to send that many more bytes. Pulling greedily therefore grants
// credit for data the consumer has not looked at yet. Bounding the loop by
// bytes limits how far ahead of actual consumption that credit can run,
// which is what keeps the amount of data buffered in JS bounded.
const kDefaultMaxBatchBytes = 65536;

async function* createBlobReaderIterable(reader, options = kEmptyObject) {
  const { maxBatchBytes = kDefaultMaxBatchBytes } = options;
  let wakeup = PromiseWithResolvers();
  let immediate;
  reader.setWakeup(() => {
    immediate ??= setImmediate(() => {
      immediate = undefined;
      wakeup.resolve?.();
    });
  });

  try {
    while (true) {
      const batch = [];
      let batchBytes = 0;
      let blocked = false;
      let eos = false;
      let error = null;

      // Pull as many chunks as available synchronously.
      // reader.pull(callback) calls the callback synchronously via
      // MakeCallback, so we can collect multiple chunks per iteration
      // step without any async overhead.
      while (true) {
        let pullResult;
        reader.pull((status, buffer) => {
          pullResult = { status, buffer };
        });

        if (pullResult.status === 0) {
          eos = true;
          break;
        }
        if (pullResult.status < 0) {
          error = new ERR_INVALID_STATE('The reader is not readable');
          break;
        }
        if (pullResult.status === 2) {
          blocked = true;
          break;
        }
        const chunk = new Uint8Array(pullResult.buffer);
        ArrayPrototypePush(batch, chunk);
        // Stop collecting once the batch is large enough. The byte budget is
        // the primary limit; the chunk count is a secondary bound so that a
        // long run of tiny chunks cannot produce an unwieldy array.
        batchBytes += chunk.byteLength;
        if (batchBytes >= maxBatchBytes || batch.length >= kMaxBatchChunks) {
          break;
        }
      }

      if (batch.length > 0) {
        yield batch;
      }

      if (eos) return;
      if (error) throw error;

      if (blocked) {
        await wakeup.promise;
        wakeup = PromiseWithResolvers();
      }
    }
  } finally {
    reader.setWakeup(undefined);
  }
}

module.exports = {
  Blob,
  blobPartsConverter,
  blobPropertyBag,
  createBlob,
  createBlobFromFilePath,
  createBlobReaderIterable,
  createBlobReaderStream,
  getBlobDataSync,
  isBlob,
  kAlreadyConverted,
  kHandle,
  kType,
  resolveObjectURL,
  TransferableBlob,
};
