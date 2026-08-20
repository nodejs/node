'use strict';

const {
  ArrayPrototypeFilter,
  MathMin,
  ObjectDefineProperties,
  ObjectKeys,
  Promise,
  SafeSet,
  StringPrototypeStartsWith,
  SymbolToStringTag,
  TypeError,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeSet,
  TypedArrayPrototypeSubarray,
  Uint32Array,
  Uint8Array,
} = primordials;

const {
  TransformStream,
} = require('internal/webstreams/transformstream');

const { customInspect } = require('internal/webstreams/util');

const {
  isArrayBufferView,
  isAnyArrayBuffer,
  isSharedArrayBuffer,
  isUint8Array,
} = require('internal/util/types');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
  setOwnProperty,
} = require('internal/util');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_STREAM_NULL_VALUES,
    ERR_TRAILING_JUNK_AFTER_STREAM_END,
  },
  genericNodeError,
} = require('internal/errors');

const { createEnumConverter } = require('internal/webidl');

const {
  Zlib,
  BrotliDecoder,
  BrotliEncoder,
} = internalBinding('zlib');

const { zlib: constants } = internalBinding('constants');
const {
  BROTLI_DECODE,
  BROTLI_ENCODE,
  BROTLI_OPERATION_FINISH,
  BROTLI_OPERATION_PROCESS,
  DEFLATE,
  DEFLATERAW,
  GUNZIP,
  GZIP,
  INFLATE,
  INFLATERAW,
  Z_DEFAULT_COMPRESSION,
  Z_DEFAULT_MEMLEVEL,
  Z_DEFAULT_STRATEGY,
  Z_DEFAULT_WINDOWBITS,
  Z_FINISH,
  Z_NO_FLUSH,
} = constants;

const { Buffer } = require('buffer');

let setImmediate;

// Output is accumulated in fixed-size buffers and emitted as soon as it is
// produced, mirroring how the zlib streams emit their output. Produced
// regions at least half a buffer large are emitted as zero-copy views (and
// the buffer is retired so no two emitted chunks ever share memory);
// smaller regions are copied out so that tiny chunks do not retain large
// allocations.
const kOutputBufferSize = 65536;
const kEmitViewThreshold = kOutputBufferSize / 2;

// Inputs larger than this are processed in slices, with a turn of the event
// loop in between, so that compressing/decompressing a huge chunk does not
// block the event loop for its full duration.
const kInputSliceSize = 65536;

// Collect all negative (error) ZLIB codes and Z_NEED_DICT.
const ZLIB_FAILURES = new SafeSet(
  ArrayPrototypeFilter(
    ObjectKeys(constants),
    (code) => code === 'Z_NEED_DICT' || constants[code] < 0,
  ),
);

// Compression error codes are surfaced as TypeError to align with the
// WHATWG Compression Streams specification.
function convertToTypeError(message, errno, code) {
  const cause = genericNodeError(message, { errno, code });
  cause.errno = errno;
  cause.code = code;
  if (ZLIB_FAILURES.has(code) ||
      // Brotli decoder error codes are formatted as 'ERR_' +
      // BrotliDecoderErrorString(), where the latter returns strings like
      // '_ERROR_FORMAT_...', '_ERROR_ALLOC_...', '_ERROR_UNREACHABLE', etc.
      // The resulting JS error codes all start with 'ERR__ERROR_'.
      StringPrototypeStartsWith(code, 'ERR__ERROR_')) {
    // eslint-disable-next-line no-restricted-syntax
    const error = new TypeError(undefined, { cause });
    setOwnProperty(error, 'code', code);
    return error;
  }
  return cause;
}

// Per the Compression Streams spec, chunks must be BufferSource
// (ArrayBuffer or ArrayBufferView not backed by SharedArrayBuffer).
// Additionally, strings are accepted for backwards compatibility with the
// previous Node.js streams-based implementation.
function normalizeChunk(chunk) {
  if (chunk === null) {
    throw new ERR_STREAM_NULL_VALUES();
  }
  if (typeof chunk === 'string') {
    return Buffer.from(chunk);
  }
  if (isArrayBufferView(chunk)) {
    if (isSharedArrayBuffer(chunk.buffer)) {
      throw new ERR_INVALID_ARG_TYPE(
        'chunk',
        ['ArrayBuffer', 'Buffer', 'TypedArray', 'DataView'],
        chunk,
      );
    }
    if (isUint8Array(chunk)) {
      return chunk;
    }
    return new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength);
  }
  if (isAnyArrayBuffer(chunk)) {
    if (isSharedArrayBuffer(chunk)) {
      throw new ERR_INVALID_ARG_TYPE(
        'chunk',
        ['ArrayBuffer', 'Buffer', 'TypedArray', 'DataView'],
        chunk,
      );
    }
    return new Uint8Array(chunk);
  }
  throw new ERR_INVALID_ARG_TYPE(
    'chunk',
    ['string', 'Buffer', 'TypedArray', 'DataView'],
    chunk,
  );
}

const formatConverter = createEnumConverter('CompressionFormat', [
  'deflate',
  'deflate-raw',
  'gzip',
  'brotli',
]);

// Processes chunks synchronously on the current thread using the raw zlib
// (or brotli) handle, avoiding both the threadpool round trip that the
// zlib streams make for every write and the stream.Duplex adapter layers.
// For the small chunks that flow through streams pipelines, running the
// compression inline is significantly cheaper than dispatching each chunk
// to the threadpool and waiting for the event loop to observe completion.
class CompressionHandle {
  #handle;
  #writeState = new Uint32Array(2);
  #error;
  #closed = false;
  #outBuffer = null;
  #outOffset = 0;
  #noFlushFlag;
  #finishFlag;
  #rejectTrailingInput;

  constructor(mode) {
    const onerror = (message, errno, code) => {
      this.#error = convertToTypeError(message, errno, code);
    };
    if (mode === BROTLI_ENCODE || mode === BROTLI_DECODE) {
      this.#handle = mode === BROTLI_DECODE ?
        new BrotliDecoder(mode) : new BrotliEncoder(mode);
      this.#handle.init(
        getBrotliDefaultParams(),
        this.#writeState,
        noopOnWriteComplete,
        undefined,
      );
      this.#noFlushFlag = BROTLI_OPERATION_PROCESS;
      this.#finishFlag = BROTLI_OPERATION_FINISH;
      this.#rejectTrailingInput = mode === BROTLI_DECODE;
    } else {
      const decompress =
        mode === INFLATE || mode === INFLATERAW || mode === GUNZIP;
      // A windowBits value of 0 tells zlib to use the window size stored
      // in the header of the compressed stream.
      const windowBits = mode === INFLATE || mode === GUNZIP ?
        0 : Z_DEFAULT_WINDOWBITS;
      this.#handle = new Zlib(mode);
      this.#handle.init(
        windowBits,
        Z_DEFAULT_COMPRESSION,
        Z_DEFAULT_MEMLEVEL,
        Z_DEFAULT_STRATEGY,
        this.#writeState,
        noopOnWriteComplete,
        undefined,
        decompress,
      );
      this.#noFlushFlag = Z_NO_FLUSH;
      this.#finishFlag = Z_FINISH;
      this.#rejectTrailingInput = decompress;
    }
    this.#handle.onerror = onerror;
  }

  transform(chunk, controller) {
    chunk = normalizeChunk(chunk);
    if (TypedArrayPrototypeGetByteLength(chunk) <= kInputSliceSize) {
      this.#process(chunk, this.#noFlushFlag, controller);
      return;
    }
    return this.#processSlices(chunk, controller);
  }

  async #processSlices(chunk, controller) {
    let offset = 0;
    while (offset < TypedArrayPrototypeGetByteLength(chunk)) {
      if (this.#closed) return;
      const end = MathMin(
        offset + kInputSliceSize, TypedArrayPrototypeGetByteLength(chunk));
      this.#process(
        TypedArrayPrototypeSubarray(chunk, offset, end),
        this.#noFlushFlag,
        controller);
      offset = end;
      if (offset < TypedArrayPrototypeGetByteLength(chunk)) {
        setImmediate ??= require('timers').setImmediate;
        await new Promise(setImmediate);
      }
    }
  }

  flush(controller) {
    try {
      this.#process(kEmptyInput, this.#finishFlag, controller);
    } finally {
      this.close();
    }
  }

  close() {
    if (!this.#closed) {
      this.#closed = true;
      this.#handle.close();
    }
  }

  #process(chunk, flushFlag, controller) {
    let availIn = TypedArrayPrototypeGetByteLength(chunk);
    let inOff = 0;
    const writeState = this.#writeState;
    const handle = this.#handle;

    let availOutAfter;
    let availInAfter;
    do {
      if (this.#outBuffer === null) {
        this.#outBuffer = new Uint8Array(kOutputBufferSize);
        this.#outOffset = 0;
      }
      const availOutBefore = kOutputBufferSize - this.#outOffset;
      handle.writeSync(flushFlag,
                       chunk, // in
                       inOff, // in_off
                       availIn, // in_len
                       this.#outBuffer, // out
                       this.#outOffset, // out_off
                       availOutBefore); // out_len
      if (this.#error !== undefined) {
        const error = this.#error;
        this.close();
        throw error;
      }

      availOutAfter = writeState[0];
      availInAfter = writeState[1];

      const have = availOutBefore - availOutAfter;
      if (have > 0) {
        this.#emit(have, controller);
      }

      // Exhausted the output buffer: emit and reprocess the rest of the
      // input against a fresh buffer.
      inOff += availIn - availInAfter;
      availIn = availInAfter;
    } while (availOutAfter === 0);

    if (availInAfter > 0 && this.#rejectTrailingInput) {
      // The compression library was not interested in receiving more data:
      // the compressed stream has ended, with junk data trailing behind it.
      const error = new ERR_TRAILING_JUNK_AFTER_STREAM_END();
      this.close();
      throw error;
    }
  }

  #emit(have, controller) {
    const offset = this.#outOffset;
    const buffer = this.#outBuffer;
    let chunk;
    if (have >= kEmitViewThreshold) {
      // Emit a zero-copy view and retire the buffer so that no two emitted
      // chunks ever share the same backing memory.
      chunk = new Uint8Array(buffer.buffer, offset, have);
      this.#outBuffer = null;
    } else {
      chunk = new Uint8Array(have);
      TypedArrayPrototypeSet(
        chunk, TypedArrayPrototypeSubarray(buffer, offset, offset + have));
      this.#outOffset = offset + have;
      if (this.#outOffset === kOutputBufferSize) {
        this.#outBuffer = null;
      }
    }
    controller.enqueue(chunk);
  }
}

const kEmptyInput = new Uint8Array(0);

// The write callback is required by the handle's init function, but it is
// only ever invoked by asynchronous writes, which are never issued here.
function noopOnWriteComplete() {}

let brotliDefaultParams;
function getBrotliDefaultParams() {
  if (brotliDefaultParams === undefined) {
    let maxParam = 0;
    for (const key of ObjectKeys(constants)) {
      if (StringPrototypeStartsWith(key, 'BROTLI_PARAM_') &&
          constants[key] > maxParam) {
        maxParam = constants[key];
      }
    }
    // -1 (as an unsigned 32-bit value) marks a parameter as unset.
    brotliDefaultParams = new Uint32Array(maxParam + 1);
    brotliDefaultParams.fill(-1);
  }
  return brotliDefaultParams;
}

// These match the strategies that the previous stream.Duplex-based
// implementation derived from the zlib streams' high water marks.
function getWritableStrategy() {
  return {
    highWaterMark: 16384,
    size(chunk) {
      return chunk?.byteLength ?? chunk?.length ?? 1;
    },
  };
}

function getReadableStrategy() {
  return {
    highWaterMark: 16384,
    size(chunk) {
      return chunk.byteLength;
    },
  };
}

function createTransform(mode) {
  const handle = new CompressionHandle(mode);
  return new TransformStream({
    transform(chunk, controller) {
      return handle.transform(chunk, controller);
    },
    flush(controller) {
      handle.flush(controller);
    },
    cancel() {
      handle.close();
    },
  }, getWritableStrategy(), getReadableStrategy());
}

/**
 * @typedef {import('./readablestream').ReadableStream} ReadableStream
 * @typedef {import('./writablestream').WritableStream} WritableStream
 */

class CompressionStream {
  #transform;

  /**
   * @param {'deflate'|'deflate-raw'|'gzip'|'brotli'} format
   */
  constructor(format) {
    format = formatConverter(format, {
      prefix: "Failed to construct 'CompressionStream'",
      context: '1st argument',
    });
    switch (format) {
      case 'deflate':
        this.#transform = createTransform(DEFLATE);
        break;
      case 'deflate-raw':
        this.#transform = createTransform(DEFLATERAW);
        break;
      case 'gzip':
        this.#transform = createTransform(GZIP);
        break;
      case 'brotli':
        this.#transform = createTransform(BROTLI_ENCODE);
        break;
    }
  }

  /**
   * @readonly
   * @type {ReadableStream}
   */
  get readable() {
    return this.#transform.readable;
  }

  /**
   * @readonly
   * @type {WritableStream}
   */
  get writable() {
    return this.#transform.writable;
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, 'CompressionStream', {
      readable: this.#transform.readable,
      writable: this.#transform.writable,
    });
  }
}

class DecompressionStream {
  #transform;

  /**
   * @param {'deflate'|'deflate-raw'|'gzip'|'brotli'} format
   */
  constructor(format) {
    format = formatConverter(format, {
      prefix: "Failed to construct 'DecompressionStream'",
      context: '1st argument',
    });
    switch (format) {
      case 'deflate':
        this.#transform = createTransform(INFLATE);
        break;
      case 'deflate-raw':
        this.#transform = createTransform(INFLATERAW);
        break;
      case 'gzip':
        this.#transform = createTransform(GUNZIP);
        break;
      case 'brotli':
        this.#transform = createTransform(BROTLI_DECODE);
        break;
    }
  }

  /**
   * @readonly
   * @type {ReadableStream}
   */
  get readable() {
    return this.#transform.readable;
  }

  /**
   * @readonly
   * @type {WritableStream}
   */
  get writable() {
    return this.#transform.writable;
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, 'DecompressionStream', {
      readable: this.#transform.readable,
      writable: this.#transform.writable,
    });
  }
}

ObjectDefineProperties(CompressionStream.prototype, {
  readable: kEnumerableProperty,
  writable: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'CompressionStream',
  },
});

ObjectDefineProperties(DecompressionStream.prototype, {
  readable: kEnumerableProperty,
  writable: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'DecompressionStream',
  },
});

module.exports = {
  CompressionStream,
  DecompressionStream,
};
