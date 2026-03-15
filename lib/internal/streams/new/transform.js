'use strict';

// New Streams API - Compression / Decompression Transforms
//
// Creates bare native zlib handles via internalBinding('zlib'), bypassing
// the stream.Transform / ZlibBase / EventEmitter machinery entirely.
// Compression runs on the libuv threadpool via handle.write() (async) so
// I/O and upstream transforms can overlap with compression work.
// Each factory returns a transform descriptor that can be passed to pull().

const {
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  MathMax,
  NumberIsNaN,
  ObjectEntries,
  ObjectKeys,
  Promise,
  SymbolAsyncIterator,
  Uint32Array,
} = primordials;

const { Buffer } = require('buffer');
const {
  genericNodeError,
} = require('internal/errors');
const { lazyDOMException } = require('internal/util');
const binding = internalBinding('zlib');
const constants = internalBinding('constants').zlib;

const {
  // Zlib modes
  DEFLATE, INFLATE, GZIP, GUNZIP,
  BROTLI_ENCODE, BROTLI_DECODE,
  ZSTD_COMPRESS, ZSTD_DECOMPRESS,
  // Zlib flush
  Z_NO_FLUSH, Z_FINISH,
  // Zlib defaults
  Z_DEFAULT_WINDOWBITS, Z_DEFAULT_COMPRESSION,
  Z_DEFAULT_MEMLEVEL, Z_DEFAULT_STRATEGY, Z_DEFAULT_CHUNK,
  // Brotli flush
  BROTLI_OPERATION_PROCESS, BROTLI_OPERATION_FINISH,
  // Zstd flush
  ZSTD_e_continue, ZSTD_e_end,
} = constants;

// ---------------------------------------------------------------------------
// Batch high water mark - yield output in chunks of approximately this size.
// ---------------------------------------------------------------------------
const BATCH_HWM = 64 * 1024;

// Pre-allocated empty buffer for flush/finalize calls.
const kEmpty = Buffer.alloc(0);

// ---------------------------------------------------------------------------
// Brotli / Zstd parameter arrays (computed once, reused per init call).
// Mirrors the pattern in lib/zlib.js.
// ---------------------------------------------------------------------------
const kMaxBrotliParam = MathMax(
  ...ObjectEntries(constants)
    .map(({ 0: key, 1: value }) =>
      (key.startsWith('BROTLI_PARAM_') ? value : 0)),
);
const brotliInitParamsArray = new Uint32Array(kMaxBrotliParam + 1);

const kMaxZstdCParam = MathMax(...ObjectKeys(constants).map(
  (key) => (key.startsWith('ZSTD_c_') ? constants[key] : 0)),
);
const zstdInitCParamsArray = new Uint32Array(kMaxZstdCParam + 1);

const kMaxZstdDParam = MathMax(...ObjectKeys(constants).map(
  (key) => (key.startsWith('ZSTD_d_') ? constants[key] : 0)),
);
const zstdInitDParamsArray = new Uint32Array(kMaxZstdDParam + 1);

// ---------------------------------------------------------------------------
// Handle creation - bare native handles, no Transform/EventEmitter overhead.
//
// Each factory accepts a processCallback (called from the threadpool
// completion path in C++) and an onError handler.
// ---------------------------------------------------------------------------

/**
 * Create a bare Zlib handle (gzip, gunzip, deflate, inflate).
 * @returns {{ handle: object, writeState: Uint32Array, chunkSize: number }}
 */
function createZlibHandle(mode, options, processCallback, onError) {
  const handle = new binding.Zlib(mode);
  const writeState = new Uint32Array(2);
  const chunkSize = options?.chunkSize ?? Z_DEFAULT_CHUNK;

  handle.onerror = onError;
  handle.init(
    options?.windowBits ?? Z_DEFAULT_WINDOWBITS,
    options?.level ?? Z_DEFAULT_COMPRESSION,
    options?.memLevel ?? Z_DEFAULT_MEMLEVEL,
    options?.strategy ?? Z_DEFAULT_STRATEGY,
    writeState,
    processCallback,
    options?.dictionary,
  );

  return { handle, writeState, chunkSize };
}

/**
 * Create a bare Brotli handle.
 * @returns {{ handle: object, writeState: Uint32Array, chunkSize: number }}
 */
function createBrotliHandle(mode, options, processCallback, onError) {
  const handle = mode === BROTLI_ENCODE ?
    new binding.BrotliEncoder(mode) : new binding.BrotliDecoder(mode);
  const writeState = new Uint32Array(2);
  const chunkSize = options?.chunkSize ?? Z_DEFAULT_CHUNK;

  brotliInitParamsArray.fill(-1);
  if (options?.params) {
    const params = options.params;
    const keys = ObjectKeys(params);
    for (let i = 0; i < keys.length; i++) {
      const key = +keys[i];
      if (!NumberIsNaN(key) && key >= 0 && key <= kMaxBrotliParam) {
        brotliInitParamsArray[key] = params[keys[i]];
      }
    }
  }

  handle.onerror = onError;
  handle.init(
    brotliInitParamsArray,
    writeState,
    processCallback,
    options?.dictionary,
  );

  return { handle, writeState, chunkSize };
}

/**
 * Create a bare Zstd handle.
 * @returns {{ handle: object, writeState: Uint32Array, chunkSize: number }}
 */
function createZstdHandle(mode, options, processCallback, onError) {
  const isCompress = mode === ZSTD_COMPRESS;
  const handle = isCompress ?
    new binding.ZstdCompress() : new binding.ZstdDecompress();
  const writeState = new Uint32Array(2);
  const chunkSize = options?.chunkSize ?? Z_DEFAULT_CHUNK;

  const initArray = isCompress ? zstdInitCParamsArray : zstdInitDParamsArray;
  const maxParam = isCompress ? kMaxZstdCParam : kMaxZstdDParam;

  initArray.fill(-1);
  if (options?.params) {
    const params = options.params;
    const keys = ObjectKeys(params);
    for (let i = 0; i < keys.length; i++) {
      const key = +keys[i];
      if (!NumberIsNaN(key) && key >= 0 && key <= maxParam) {
        initArray[key] = params[keys[i]];
      }
    }
  }

  handle.onerror = onError;
  handle.init(
    initArray,
    options?.pledgedSrcSize,
    writeState,
    processCallback,
    options?.dictionary,
  );

  return { handle, writeState, chunkSize };
}

// ---------------------------------------------------------------------------
// Core: makeZlibTransform
//
// Uses async handle.write() so compression runs on the libuv threadpool.
// The generator manually iterates the source with pre-reading: the next
// upstream read+transform is started before awaiting the current compression,
// so I/O and upstream work overlap with threadpool compression.
// ---------------------------------------------------------------------------
function makeZlibTransform(createHandleFn, processFlag, finishFlag) {
  return {
    transform: async function*(source, options) {
      const { signal } = options;

      // Fail fast if already aborted - don't allocate a native handle.
      if (signal.aborted) {
        throw signal.reason ??
              lazyDOMException('The operation was aborted', 'AbortError');
      }

      // ---- Per-invocation state shared with the write callback ----
      let outBuf;
      let outOffset = 0;
      let chunkSize;
      const pending = [];
      let pendingBytes = 0;

      // Current write operation state (read by the callback for looping).
      let resolveWrite, rejectWrite;
      let writeInput, writeFlush;
      let writeInOff, writeAvailIn, writeAvailOutBefore;

      // processCallback: called by C++ AfterThreadPoolWork when compression
      // on the threadpool completes. Collects output, loops if the engine
      // has more output to produce (availOut === 0), then resolves the
      // promise when all output for this input chunk is collected.
      function onWriteComplete() {
        const availOut = writeState[0];
        const availInAfter = writeState[1];
        const have = writeAvailOutBefore - availOut;

        if (have > 0) {
          ArrayPrototypePush(pending,
                             outBuf.slice(outOffset, outOffset + have));
          pendingBytes += have;
          outOffset += have;
        }

        // Reallocate output buffer if exhausted.
        if (availOut === 0 || outOffset >= chunkSize) {
          outBuf = Buffer.allocUnsafe(chunkSize);
          outOffset = 0;
        }

        if (availOut === 0) {
          // Engine has more output - but if aborted, don't loop.
          if (!resolveWrite) return;

          const consumed = writeAvailIn - availInAfter;
          writeInOff += consumed;
          writeAvailIn = availInAfter;
          writeAvailOutBefore = chunkSize - outOffset;

          handle.write(writeFlush,
                       writeInput, writeInOff, writeAvailIn,
                       outBuf, outOffset, writeAvailOutBefore);
          return; // Will call onWriteComplete again.
        }

        // All input consumed and output collected.
        handle.buffer = null;
        const resolve = resolveWrite;
        resolveWrite = undefined;
        rejectWrite = undefined;
        if (resolve) resolve();
      }

      // onError: called by C++ when the engine encounters an error.
      // Fires instead of onWriteComplete - reject the promise.
      function onError(message, errno, code) {
        const error = genericNodeError(message, { errno, code });
        error.errno = errno;
        error.code = code;
        const reject = rejectWrite;
        resolveWrite = undefined;
        rejectWrite = undefined;
        if (reject) reject(error);
      }

      // ---- Create the handle with our callbacks ----
      const result = createHandleFn(onWriteComplete, onError);
      const handle = result.handle;
      const writeState = result.writeState;
      chunkSize = result.chunkSize;
      outBuf = Buffer.allocUnsafe(chunkSize);

      // Abort handler: reject any in-flight threadpool operation so the
      // generator doesn't block waiting for compression to finish.
      const onAbort = () => {
        const reject = rejectWrite;
        resolveWrite = undefined;
        rejectWrite = undefined;
        if (reject) {
          reject(signal.reason ??
                 lazyDOMException('The operation was aborted', 'AbortError'));
        }
      };
      signal.addEventListener('abort', onAbort, { once: true });

      // Dispatch input to the threadpool and return a promise.
      function processInputAsync(input, flushFlag) {
        return new Promise((resolve, reject) => {
          resolveWrite = resolve;
          rejectWrite = reject;
          writeInput = input;
          writeFlush = flushFlag;
          writeInOff = 0;
          writeAvailIn = input.byteLength;
          writeAvailOutBefore = chunkSize - outOffset;

          // Keep input alive while the threadpool references it.
          handle.buffer = input;

          handle.write(flushFlag,
                       input, 0, input.byteLength,
                       outBuf, outOffset, writeAvailOutBefore);
        });
      }

      function drainBatch() {
        if (pendingBytes <= BATCH_HWM) {
          const batch = ArrayPrototypeSplice(pending, 0, pending.length);
          pendingBytes = 0;
          return batch;
        }
        const batch = [];
        let batchBytes = 0;
        while (pending.length > 0 && batchBytes < BATCH_HWM) {
          const buf = pending.shift();
          ArrayPrototypePush(batch, buf);
          batchBytes += buf.byteLength;
          pendingBytes -= buf.byteLength;
        }
        return batch;
      }

      let finalized = false;

      const iter = source[SymbolAsyncIterator]();
      try {
        // Manually iterate the source so we can pre-read: calling
        // iter.next() starts the upstream read + transform on libuv
        // before we await the current compression on the threadpool.
        let nextResult = iter.next();

        while (true) {
          const { value: chunks, done } = await nextResult;
          if (done) break;

          if (signal.aborted) {
            throw signal.reason ??
                  lazyDOMException('The operation was aborted', 'AbortError');
          }

          if (chunks === null) {
            // Flush signal - finalize the engine.
            if (!finalized) {
              finalized = true;
              await processInputAsync(kEmpty, finishFlag);
              while (pending.length > 0) {
                yield drainBatch();
              }
            }
            nextResult = iter.next();
            continue;
          }

          // Pre-read: start upstream I/O + transform for the NEXT batch
          // while we compress the current batch on the threadpool.
          nextResult = iter.next();

          for (let i = 0; i < chunks.length; i++) {
            await processInputAsync(chunks[i], processFlag);
          }

          if (pendingBytes >= BATCH_HWM) {
            while (pending.length > 0 && pendingBytes >= BATCH_HWM) {
              yield drainBatch();
            }
          }
          if (pending.length > 0) {
            yield drainBatch();
          }
        }

        // Source ended - finalize if not already done by a null signal.
        if (!finalized && !signal.aborted) {
          finalized = true;
          await processInputAsync(kEmpty, finishFlag);
          while (pending.length > 0) {
            yield drainBatch();
          }
        }
      } finally {
        signal.removeEventListener('abort', onAbort);
        handle.close();
        // Close the upstream iterator so its finally blocks run promptly
        // rather than waiting for GC.
        try { await iter.return?.(); } catch { /* Intentional no-op. */ }
      }
    },
  };
}

// ---------------------------------------------------------------------------
// Compression factories
// ---------------------------------------------------------------------------

function compressGzip(options) {
  return makeZlibTransform(
    (cb, onErr) => createZlibHandle(GZIP, options, cb, onErr),
    Z_NO_FLUSH, Z_FINISH,
  );
}

function compressDeflate(options) {
  return makeZlibTransform(
    (cb, onErr) => createZlibHandle(DEFLATE, options, cb, onErr),
    Z_NO_FLUSH, Z_FINISH,
  );
}

function compressBrotli(options) {
  return makeZlibTransform(
    (cb, onErr) => createBrotliHandle(BROTLI_ENCODE, options, cb, onErr),
    BROTLI_OPERATION_PROCESS, BROTLI_OPERATION_FINISH,
  );
}

function compressZstd(options) {
  return makeZlibTransform(
    (cb, onErr) => createZstdHandle(ZSTD_COMPRESS, options, cb, onErr),
    ZSTD_e_continue, ZSTD_e_end,
  );
}

// ---------------------------------------------------------------------------
// Decompression factories
// ---------------------------------------------------------------------------

function decompressGzip(options) {
  return makeZlibTransform(
    (cb, onErr) => createZlibHandle(GUNZIP, options, cb, onErr),
    Z_NO_FLUSH, Z_FINISH,
  );
}

function decompressDeflate(options) {
  return makeZlibTransform(
    (cb, onErr) => createZlibHandle(INFLATE, options, cb, onErr),
    Z_NO_FLUSH, Z_FINISH,
  );
}

function decompressBrotli(options) {
  return makeZlibTransform(
    (cb, onErr) => createBrotliHandle(BROTLI_DECODE, options, cb, onErr),
    BROTLI_OPERATION_PROCESS, BROTLI_OPERATION_FINISH,
  );
}

function decompressZstd(options) {
  return makeZlibTransform(
    (cb, onErr) => createZstdHandle(ZSTD_DECOMPRESS, options, cb, onErr),
    ZSTD_e_continue, ZSTD_e_end,
  );
}

module.exports = {
  compressGzip,
  compressDeflate,
  compressBrotli,
  compressZstd,
  decompressGzip,
  decompressDeflate,
  decompressBrotli,
  decompressZstd,
};
