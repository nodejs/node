'use strict';

const {
  ArrayPrototypePop,
  ArrayPrototypePush,
  Error,
  ErrorCaptureStackTrace,
  FunctionPrototypeBind,
  MathMax,
  MathMin,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  SafeArrayIterator,
  SafePromisePrototypeFinally,
  Symbol,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
  Uint8Array,
  uncurryThis,
} = primordials;

const { fs: constants } = internalBinding('constants');
const {
  F_OK,
  O_SYMLINK,
  O_WRONLY,
  S_IFMT,
  S_IFREG,
} = constants;

const binding = internalBinding('fs');
const { Buffer } = require('buffer');
const { isBuffer: BufferIsBuffer } = Buffer;
const BufferToString = uncurryThis(Buffer.prototype.toString);

const {
  AbortError,
  aggregateTwoErrors,
  codes: {
    ERR_ACCESS_DENIED,
    ERR_FS_FILE_TOO_LARGE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_METHOD_NOT_IMPLEMENTED,
    ERR_OPERATION_FAILED,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const { isArrayBufferView } = require('internal/util/types');

const {
  constants: {
    kIoMaxLength,
    kMaxUserId,
    kReadFileBufferLength,
    kReadFileUnknownBufferLength,
    kWriteFileMaxChunkSize,
  },
  copyObject,
  getDirents,
  getOptions,
  getStatFsFromBinding,
  getStatsFromBinding,
  getValidatedPath,
  preprocessSymlinkDestination,
  stringToFlags,
  stringToSymlinkType,
  toUnixTimestamp,
  handleErrorFromBinding: handleSyncErrorFromBinding,
  validateBufferArray,
  validateCpOptions,
  validateOffsetLengthRead,
  validateOffsetLengthWrite,
  validatePosition,
  validateRmOptions,
  validateRmdirOptions,
  validateStringAfterArrayBufferView,
  warnOnNonPortableTemplate,
} = require('internal/fs/utils');
const { opendir } = require('internal/fs/dir');
const {
  parseFileMode,
  validateAbortSignal,
  validateBoolean,
  validateBuffer,
  validateEncoding,
  validateInteger,
  validateObject,
  validateOneOf,
  kValidateObjectAllowNullable,
} = require('internal/validators');
const pathModule = require('path');
const {
  getLazy,
  kEmptyObject,
  lazyDOMException,
  promisify,
  isWindows,
  isMacOS,
} = require('internal/util');
const { getOptionValue } = require('internal/options');
const EventEmitter = require('events');
const { StringDecoder } = require('string_decoder');
const { kFSWatchStart, watch } = require('internal/fs/watchers');
const nonNativeWatcher = require('internal/fs/recursive_watch');
const { isIterable } = require('internal/streams/utils');
const assert = require('internal/assert');

const permission = require('internal/process/permission');

const kHandle = Symbol('kHandle');
const kFd = Symbol('kFd');
const kRefs = Symbol('kRefs');
const kClosePromise = Symbol('kClosePromise');
const kCloseReason = Symbol('kCloseReason');
const kCloseResolve = Symbol('kCloseResolve');
const kCloseReject = Symbol('kCloseReject');
const kRef = Symbol('kRef');
const kUnref = Symbol('kUnref');
const kLocked = Symbol('kLocked');
const kCloseSync = Symbol('kCloseSync');

const { kUsePromises } = binding;
const { Interface } = require('internal/readline/interface');
const {
  kDeserialize, kTransfer, kTransferList, markTransferMode,
} = require('internal/worker/js_transferable');

const getDirectoryEntriesPromise = promisify(getDirents);
const validateRmOptionsPromise = promisify(validateRmOptions);

let cpPromises;
function lazyLoadCpPromises() {
  return cpPromises ??= require('internal/fs/cp/cp').cpFn;
}

// Lazy loaded to avoid circular dependency.
let fsStreams;
function lazyFsStreams() {
  return fsStreams ??= require('internal/fs/streams');
}

const lazyRimRaf = getLazy(() => require('internal/fs/rimraf').rimrafPromises);

const lazyReadableStream = getLazy(() =>
  require('internal/webstreams/readablestream').ReadableStream,
);

// Lazy loaded to avoid circular dependency with new streams.
let newStreamsPull;
let newStreamsPullSync;
let newStreamsParsePullArgs;
let newStreamsToUint8Array;
let newStreamsConvertChunks;
function lazyNewStreams() {
  if (newStreamsPull === undefined) {
    const pullModule = require('internal/streams/iter/pull');
    newStreamsPull = pullModule.pull;
    newStreamsPullSync = pullModule.pullSync;
    const utils = require('internal/streams/iter/utils');
    newStreamsParsePullArgs = utils.parsePullArgs;
    newStreamsToUint8Array = utils.toUint8Array;
    newStreamsConvertChunks = utils.convertChunks;
  }
}

// By the time the C++ land creates an error for a promise rejection (likely from a
// libuv callback), there is already no JS frames on the stack. So we need to
// wait until V8 resumes execution back to JS land before we have enough information
// to re-capture the stack trace.
function handleErrorFromBinding(error) {
  ErrorCaptureStackTrace(error, handleErrorFromBinding);
  return PromiseReject(error);
}

class FileHandle extends EventEmitter {
  /**
   * @param {InternalFSBinding.FileHandle | undefined} filehandle
   */
  constructor(filehandle) {
    super();
    markTransferMode(this, false, true);
    this[kHandle] = filehandle;
    this[kFd] = filehandle ? filehandle.fd : -1;

    this[kRefs] = 1;
    this[kClosePromise] = null;
  }

  getAsyncId() {
    return this[kHandle].getAsyncId();
  }

  get fd() {
    return this[kFd];
  }

  appendFile(data, options) {
    return fsCall(writeFile, this, data, options);
  }

  chmod(mode) {
    return fsCall(fchmod, this, mode);
  }

  chown(uid, gid) {
    return fsCall(fchown, this, uid, gid);
  }

  datasync() {
    return fsCall(fdatasync, this);
  }

  sync() {
    return fsCall(fsync, this);
  }

  read(buffer, offset, length, position) {
    return fsCall(read, this, buffer, offset, length, position);
  }

  readv(buffers, position) {
    return fsCall(readv, this, buffers, position);
  }

  readFile(options) {
    return fsCall(readFile, this, options);
  }

  readLines(options = undefined) {
    return new Interface({
      input: this.createReadStream(options),
      crlfDelay: Infinity,
    });
  }

  stat(options) {
    return fsCall(fstat, this, options);
  }

  truncate(len = 0) {
    return fsCall(ftruncate, this, len);
  }

  utimes(atime, mtime) {
    return fsCall(futimes, this, atime, mtime);
  }

  write(buffer, offset, length, position) {
    return fsCall(write, this, buffer, offset, length, position);
  }

  writev(buffers, position) {
    return fsCall(writev, this, buffers, position);
  }

  writeFile(data, options) {
    return fsCall(writeFile, this, data, options);
  }

  close = () => {
    if (this[kFd] === -1) {
      return PromiseResolve();
    }

    if (this[kClosePromise]) {
      return this[kClosePromise];
    }

    this[kRefs]--;
    if (this[kRefs] === 0) {
      this[kFd] = -1;
      this[kClosePromise] = SafePromisePrototypeFinally(
        this[kHandle].close(),
        () => { this[kClosePromise] = undefined; },
      );
    } else {
      this[kClosePromise] = SafePromisePrototypeFinally(
        new Promise((resolve, reject) => {
          this[kCloseResolve] = resolve;
          this[kCloseReject] = reject;
        }), () => {
          this[kClosePromise] = undefined;
          this[kCloseReject] = undefined;
          this[kCloseResolve] = undefined;
        },
      );
    }

    this.emit('close');
    return this[kClosePromise];
  };

  [kCloseSync]() {
    if (this[kFd] === -1) return;
    if (this[kClosePromise]) {
      throw new ERR_INVALID_STATE('The FileHandle is closing');
    }
    this[kFd] = -1;
    this[kHandle].closeSync();
    this.emit('close');
  }

  async [SymbolAsyncDispose]() {
    await this.close();
  }

  /**
   * @typedef {import('../webstreams/readablestream').ReadableStream
   * } ReadableStream
   * @param {{ type?: 'bytes', autoClose?: boolean }} [options]
   * @returns {ReadableStream}
   */
  readableWebStream(options = kEmptyObject) {
    if (this[kFd] === -1)
      throw new ERR_INVALID_STATE('The FileHandle is closed');
    if (this[kClosePromise])
      throw new ERR_INVALID_STATE('The FileHandle is closing');
    if (this[kLocked])
      throw new ERR_INVALID_STATE('The FileHandle is locked');
    this[kLocked] = true;

    validateObject(options, 'options');
    const {
      type = 'bytes',
      autoClose = false,
    } = options;

    validateBoolean(autoClose, 'options.autoClose');

    if (type !== 'bytes') {
      process.emitWarning(
        'A non-"bytes" options.type has no effect. A byte-oriented steam is ' +
        'always created.',
        'ExperimentalWarning',
      );
    }

    const readFn = FunctionPrototypeBind(this.read, this);
    const ondone = async () => {
      this[kUnref]();
      if (autoClose) await this.close();
    };

    const ReadableStream = lazyReadableStream();
    const readable = new ReadableStream({
      type: 'bytes',
      autoAllocateChunkSize: 16384,

      async pull(controller) {
        const view = controller.byobRequest.view;
        const { bytesRead } = await readFn(view, view.byteOffset, view.byteLength);

        if (bytesRead === 0) {
          controller.close();
          await ondone();
        }

        controller.byobRequest.respond(bytesRead);
      },

      async cancel() {
        await ondone();
      },
    });


    const {
      readableStreamCancel,
    } = require('internal/webstreams/readablestream');
    this[kRef]();
    this.once('close', () => {
      readableStreamCancel(readable);
    });

    return readable;
  }

  /**
   * @typedef {import('./streams').ReadStream
   * } ReadStream
   * @param {{
   *   encoding?: string;
   *   autoClose?: boolean;
   *   emitClose?: boolean;
   *   start: number;
   *   end?: number;
   *   highWaterMark?: number;
   *   }} [options]
   * @returns {ReadStream}
   */
  createReadStream(options = undefined) {
    const { ReadStream } = lazyFsStreams();
    return new ReadStream(undefined, { ...options, fd: this });
  }

  /**
   * @typedef {import('./streams').WriteStream
   * } WriteStream
   * @param {{
   *   encoding?: string;
   *   autoClose?: boolean;
   *   emitClose?: boolean;
   *   start: number;
   *   highWaterMark?: number;
   *   flush?: boolean;
   *   }} [options]
   * @returns {WriteStream}
   */
  createWriteStream(options = undefined) {
    const { WriteStream } = lazyFsStreams();
    return new WriteStream(undefined, { ...options, fd: this });
  }

  [kTransfer]() {
    if (this[kClosePromise] || this[kRefs] > 1) {
      throw lazyDOMException('Cannot transfer FileHandle while in use',
                             'DataCloneError');
    }

    const handle = this[kHandle];
    this[kFd] = -1;
    this[kCloseReason] = 'The FileHandle has been transferred';
    this[kHandle] = null;
    this[kRefs] = 0;

    return {
      data: { handle },
      deserializeInfo: 'internal/fs/promises:FileHandle',
    };
  }

  [kTransferList]() {
    return [ this[kHandle] ];
  }

  [kDeserialize]({ handle }) {
    this[kHandle] = handle;
    this[kFd] = handle.fd;
  }

  [kRef]() {
    this[kRefs]++;
  }

  [kUnref]() {
    this[kRefs]--;
    if (this[kRefs] === 0) {
      this[kFd] = -1;
      PromisePrototypeThen(
        this[kHandle].close(),
        this[kCloseResolve],
        this[kCloseReject],
      );
    }
  }
}

if (getOptionValue('--experimental-stream-iter')) {
  const kNullPrototo = { __proto__: null };
  const kDefaultChunkSize = 131072;
  const kNone = -1;
  /**
   * Return the file contents as an AsyncIterable<Uint8Array[]> using the
   * new streams pull model. Optional transforms and options (including
   * AbortSignal) may be provided as trailing arguments, mirroring the
   * Stream.pull() signature.
   * @param {...(Function|object)} args - Optional transforms and/or options
   * @returns {AsyncIterable<Uint8Array[]>}
   */
  FileHandle.prototype.pull = function pull(...args) {
    if (this[kFd] === kNone)
      throw new ERR_INVALID_STATE('The FileHandle is closed');
    if (this[kClosePromise])
      throw new ERR_INVALID_STATE('The FileHandle is closing');
    if (this[kLocked])
      throw new ERR_INVALID_STATE('The FileHandle is locked');

    lazyNewStreams();
    const { transforms, options = kNullPrototo } = newStreamsParsePullArgs(args);

    const {
      autoClose = false,
      chunkSize: readSize = kDefaultChunkSize,
      signal,
    } = options;
    let {
      start: pos = kNone,
      limit: remaining = kNone,
    } = options;

    const handle = this;
    const fd = this[kFd];

    validateBoolean(autoClose, 'options.autoClose');

    if (pos !== kNone) {
      validateInteger(pos, 'options.start', 0);
    }
    if (remaining !== kNone) {
      validateInteger(remaining, 'options.limit', 1);
    }
    if (readSize !== undefined) {
      validateInteger(readSize, 'options.chunkSize', 1);
    }
    if (signal !== undefined) {
      validateAbortSignal(signal, 'options.signal');
    }

    this[kLocked] = true;

    const source = {
      __proto__: null,
      async *[SymbolAsyncIterator]() {
        handle[kRef]();
        try {
          if (signal) {
            // Signal-aware path
            while (remaining !== 0) {
              if (signal.aborted) {
                throw signal.reason ??
                      lazyDOMException('The operation was aborted',
                                       'AbortError');
              }
              const toRead = remaining > 0 ?
                MathMin(readSize, remaining) : readSize;
              const buf = Buffer.allocUnsafe(toRead);
              let bytesRead;
              try {
                bytesRead =
                  (await binding.read(fd, buf, 0,
                                      toRead, pos, kUsePromises)) || 0;
              } catch (err) {
                ErrorCaptureStackTrace(err, handleErrorFromBinding);
                throw err;
              }
              if (bytesRead === 0) break;
              if (pos >= 0) pos += bytesRead;
              if (remaining > 0) remaining -= bytesRead;
              yield [bytesRead < toRead ? buf.subarray(0, bytesRead) : buf];
            }
          } else {
            // Fast path - no signal check per iteration
            while (remaining !== 0) {
              const toRead = remaining > 0 ?
                MathMin(readSize, remaining) : readSize;
              const buf = Buffer.allocUnsafe(toRead);
              let bytesRead;
              try {
                bytesRead =
                  (await binding.read(fd, buf, 0,
                                      toRead, pos, kUsePromises)) || 0;
              } catch (err) {
                ErrorCaptureStackTrace(err, handleErrorFromBinding);
                throw err;
              }
              if (bytesRead === 0) break;
              if (pos >= 0) pos += bytesRead;
              if (remaining > 0) remaining -= bytesRead;
              yield [bytesRead < toRead ? buf.subarray(0, bytesRead) : buf];
            }
          }
        } finally {
          handle[kLocked] = false;
          handle[kUnref]();
          if (autoClose) {
            await handle.close();
          }
        }
      },
    };

    // If transforms provided, wrap with pull pipeline
    if (transforms.length > 0) {
      const pullArgs = [...transforms];
      if (options) {
        ArrayPrototypePush(pullArgs, options);
      }
      return newStreamsPull(source, ...pullArgs);
    }
    return source;
  };

  /**
   * Return the file contents as an Iterable<Uint8Array[]> using synchronous
   * reads. Optional transforms and options may be provided as trailing
   * arguments, mirroring the Stream.pullSync() signature.
   * @param {...(Function|object)} args - Optional transforms and/or options
   * @returns {Iterable<Uint8Array[]>}
   */
  FileHandle.prototype.pullSync = function pullSync(...args) {
    if (this[kFd] === kNone)
      throw new ERR_INVALID_STATE('The FileHandle is closed');
    if (this[kClosePromise])
      throw new ERR_INVALID_STATE('The FileHandle is closing');
    if (this[kLocked])
      throw new ERR_INVALID_STATE('The FileHandle is locked');

    lazyNewStreams();
    const { transforms, options = kNullPrototo } = newStreamsParsePullArgs(args);

    const {
      autoClose = false,
      chunkSize: readSize = kDefaultChunkSize,
    } = options;
    let {
      start: pos = kNone,
      limit: remaining = kNone,
    } = options;

    const handle = this;
    const fd = this[kFd];

    validateBoolean(autoClose, 'options.autoClose');

    if (pos !== kNone) {
      validateInteger(pos, 'options.start', 0);
    }
    if (remaining !== kNone) {
      validateInteger(remaining, 'options.limit', 1);
    }
    if (readSize !== undefined) {
      validateInteger(readSize, 'options.chunkSize', 1);
    }

    this[kLocked] = true;

    handle[kRef]();

    function cleanup() {
      handle[kLocked] = false;
      handle[kUnref]();
      if (autoClose) {
        handle[kCloseSync]();
      }
    }

    const source = {
      __proto__: null,
      [SymbolIterator]() {
        let done = false;
        return {
          __proto__: null,
          next() {
            if (done || remaining === 0) {
              if (!done) {
                done = true;
                cleanup();
              }
              return { value: undefined, done: true };
            }
            const toRead = remaining > 0 ?
              MathMin(readSize, remaining) : readSize;
            const buf = Buffer.allocUnsafe(toRead);
            let bytesRead;
            try {
              bytesRead = binding.read(fd, buf, 0, toRead, pos) || 0;
            } catch (err) {
              done = true;
              cleanup();
              throw err;
            }
            if (bytesRead === 0) {
              done = true;
              cleanup();
              return { value: undefined, done: true };
            }
            if (pos >= 0) pos += bytesRead;
            if (remaining > 0) remaining -= bytesRead;
            const chunk = bytesRead < toRead ?
              buf.subarray(0, bytesRead) : buf;
            return { value: [chunk], done: false };
          },
          return() {
            if (!done) {
              done = true;
              cleanup();
            }
            return { value: undefined, done: true };
          },
        };
      },
    };

    if (transforms.length > 0) {
      return newStreamsPullSync(source, ...transforms);
    }
    return source;
  };

  /**
   * Return a new-streams Writer backed by this file handle.
   * The writer uses direct binding.writeBuffer / binding.writeBuffers
   * calls, bypassing the FileHandle.write() validation chain.
   *
   * Supports writev() for batch writes (single syscall per batch).
   * Handles EAGAIN with retry (up to 5 attempts), matching WriteStream.
   * @param {{
   *   autoClose?: boolean;
   *   start?: number;
   * }} [options]
   * @returns {{ write, writev, end, fail }}
   */
  FileHandle.prototype.writer = function writer(options = kNullPrototo) {
    if (this[kFd] === kNone)
      throw new ERR_INVALID_STATE('The FileHandle is closed');
    if (this[kClosePromise])
      throw new ERR_INVALID_STATE('The FileHandle is closing');
    if (this[kLocked])
      throw new ERR_INVALID_STATE('The FileHandle is locked');

    lazyNewStreams();

    validateObject(options, 'options');
    const {
      autoClose = false,
      chunkSize: syncWriteThreshold = kDefaultChunkSize,
    } = options;
    let {
      start: pos = kNone,
      limit: bytesRemaining = kNone,
    } = options;

    const handle = this;
    const fd = this[kFd];
    let totalBytesWritten = 0;
    let closed = false;
    let closing = false;
    let pendingEndPromise = null;
    let error = null;
    let asyncPending = false;

    validateBoolean(autoClose, 'options.autoClose');

    if (pos !== kNone) {
      validateInteger(pos, 'options.start', 0);
    }
    if (bytesRemaining !== kNone) {
      validateInteger(bytesRemaining, 'options.limit', 1);
    }
    if (syncWriteThreshold !== undefined) {
      validateInteger(syncWriteThreshold, 'options.chunkSize', 1);
    }

    this[kLocked] = true;
    handle[kRef]();

    // Write a single buffer with EAGAIN retry (up to 5 retries).
    async function writeAll(buf, offset, length, position, signal) {
      asyncPending = true;
      try {
        let retries = 0;
        while (length > 0) {
          const bytesWritten = (await PromisePrototypeThen(
            binding.writeBuffer(fd, buf, offset, length, position,
                                kUsePromises),
            undefined,
            handleErrorFromBinding,
          )) || 0;

          signal?.throwIfAborted();

          if (bytesWritten === 0) {
            if (++retries > 5) {
              throw new ERR_OPERATION_FAILED('write failed after retries');
            }
          } else {
            retries = 0;
          }

          totalBytesWritten += bytesWritten;
          offset += bytesWritten;
          length -= bytesWritten;
          if (position >= 0) position += bytesWritten;
        }
      } finally {
        asyncPending = false;
      }
    }

    // Writev with EAGAIN retry. On partial write, concatenates remaining
    // buffers and falls back to writeAll (same approach as WriteStream).
    async function writevAll(buffers, position, signal) {
      asyncPending = true;
      try {
        let totalSize = 0;
        for (let i = 0; i < buffers.length; i++) {
          totalSize += buffers[i].byteLength;
        }

        let retries = 0;
        while (totalSize > 0) {
          const bytesWritten = (await PromisePrototypeThen(
            binding.writeBuffers(fd, buffers, position, kUsePromises),
            undefined,
            handleErrorFromBinding,
          )) || 0;

          signal?.throwIfAborted();

          if (bytesWritten === 0) {
            if (++retries > 5) {
              throw new ERR_OPERATION_FAILED('writev failed after retries');
            }
          } else {
            retries = 0;
          }

          totalBytesWritten += bytesWritten;
          totalSize -= bytesWritten;
          if (position >= 0) position += bytesWritten;

          if (totalSize > 0) {
            // Partial write - concatenate remaining and use writeAll.
            const remaining = Buffer.concat(buffers);
            const wrote = bytesWritten;
            // writeAll is already inside asyncPending = true, but
            // writeAll sets it again - that's fine (idempotent).
            await writeAll(remaining, wrote, remaining.length - wrote,
                           position, signal);
            return;
          }
        }
      } finally {
        asyncPending = false;
      }
    }

    // Synchronous write with EAGAIN retry. Throws on I/O error.
    // Used by writeSync for the full write, and by writevSync for
    // completing a partial writev.
    function writeSyncAll(buf, offset, length, position) {
      let retries = 0;
      while (length > 0) {
        const ctx = {};
        const bytesWritten = binding.writeBuffer(
          fd, buf, offset, length, position, undefined, ctx) || 0;
        if (ctx.errno !== undefined) {
          handleSyncErrorFromBinding(ctx);
        }
        if (bytesWritten === 0) {
          if (++retries > 5) {
            throw new ERR_OPERATION_FAILED('write failed after retries');
          }
        } else {
          retries = 0;
        }
        totalBytesWritten += bytesWritten;
        offset += bytesWritten;
        length -= bytesWritten;
        if (position >= 0) position += bytesWritten;
      }
    }

    async function cleanup() {
      if (closed) return;
      closed = true;
      handle[kLocked] = false;
      handle[kUnref]();
      if (autoClose) {
        await handle.close();
      }
    }

    return {
      __proto__: null,
      write(chunk, options = kNullPrototo) {
        if (error) {
          return PromiseReject(error);
        }
        if (closed) {
          return PromiseReject(
            new ERR_INVALID_STATE.TypeError('The writer is closed'));
        }
        validateObject(options, 'options');
        const {
          signal,
        } = options;
        if (signal !== undefined) {
          validateAbortSignal(signal, 'options.signal');
          if (signal.aborted) {
            return PromiseReject(signal.reason);
          }
        }
        chunk = newStreamsToUint8Array(chunk);
        if (bytesRemaining >= 0 && chunk.byteLength > bytesRemaining) {
          return PromiseReject(
            new ERR_OUT_OF_RANGE('write', `<= ${bytesRemaining} bytes`,
                                 chunk.byteLength));
        }
        if (bytesRemaining > 0) bytesRemaining -= chunk.byteLength;
        const position = pos;
        if (pos >= 0) pos += chunk.byteLength;
        return writeAll(chunk, 0, chunk.byteLength, position, signal);
      },

      writev(chunks, options = kNullPrototo) {
        if (error) {
          return PromiseReject(error);
        }
        if (closed) {
          return PromiseReject(
            new ERR_INVALID_STATE.TypeError('The writer is closed'));
        }
        validateObject(options, 'options');
        const {
          signal,
        } = options;
        if (signal !== undefined) {
          validateAbortSignal(signal, 'options.signal');
          if (signal?.aborted) {
            return PromiseReject(signal.reason);
          }
        }
        chunks = newStreamsConvertChunks(chunks);
        let totalSize = 0;
        for (let i = 0; i < chunks.length; i++) {
          totalSize += chunks[i].byteLength;
        }
        if (bytesRemaining >= 0 && totalSize > bytesRemaining) {
          return PromiseReject(
            new ERR_OUT_OF_RANGE('writev', `<= ${bytesRemaining} bytes`,
                                 totalSize));
        }
        if (bytesRemaining > 0) bytesRemaining -= totalSize;
        const position = pos;
        if (pos >= 0) pos += totalSize;
        return writevAll(chunks, position, signal);
      },

      writeSync(chunk) {
        if (error || closed || asyncPending) return false;
        chunk = newStreamsToUint8Array(chunk);
        const length = chunk.byteLength;
        if (length > syncWriteThreshold) return false;
        if (length === 0) return true;
        if (bytesRemaining >= 0 && length > bytesRemaining) return false;
        const position = pos;
        // First attempt - if this fails with zero bytes written,
        // return false so pipeTo can fall back to async write().
        const ctx = {};
        const bytesWritten = binding.writeBuffer(
          fd, chunk, 0, length, position, undefined, ctx) || 0;
        if (ctx.errno !== undefined) return false;
        totalBytesWritten += bytesWritten;
        if (position >= 0) {
          pos = position + bytesWritten;
        }
        if (bytesWritten === length) {
          if (bytesRemaining > 0) bytesRemaining -= length;
          return true;
        }
        // Partial write - bytes are on disk. Must complete or throw.
        // Cannot return false here because pipeTo would re-send the
        // full chunk, causing duplicate data on disk.
        writeSyncAll(chunk, bytesWritten, length - bytesWritten,
                     position >= 0 ? position + bytesWritten : -1);
        if (bytesRemaining > 0) bytesRemaining -= length;
        return true;
      },

      writevSync(chunks) {
        if (error || closed || asyncPending) return false;
        chunks = newStreamsConvertChunks(chunks);
        let totalSize = 0;
        for (let i = 0; i < chunks.length; i++) {
          totalSize += chunks[i].byteLength;
        }
        if (totalSize > syncWriteThreshold) return false;
        if (totalSize === 0) return true;
        if (bytesRemaining >= 0 && totalSize > bytesRemaining) return false;
        const position = pos;
        // writeBuffers throws on error (zero bytes written) - safe
        // to catch and return false for async fallback.
        let bytesWritten;
        try {
          bytesWritten = binding.writeBuffers(fd, chunks, position) || 0;
        } catch {
          return false;
        }
        totalBytesWritten += bytesWritten;
        if (position >= 0) {
          pos = position + bytesWritten;
        }
        if (bytesWritten === totalSize) {
          if (bytesRemaining > 0) bytesRemaining -= totalSize;
          return true;
        }
        // Partial writev - bytes are on disk. Must complete or throw.
        const rest = Buffer.concat(chunks);
        writeSyncAll(rest, bytesWritten,
                     rest.byteLength - bytesWritten,
                     position >= 0 ? position + bytesWritten : -1);
        if (bytesRemaining > 0) bytesRemaining -= totalSize;
        return true;
      },

      end(options = kNullPrototo) {
        if (error) {
          return PromiseReject(error);
        }
        if (closed) {
          return PromiseResolve(totalBytesWritten);
        }
        if (closing) {
          return pendingEndPromise;
        }
        validateObject(options, 'options');
        const {
          signal,
        } = options;
        if (signal !== undefined) {
          validateAbortSignal(signal, 'options.signal');
          if (signal.aborted) {
            return PromiseReject(signal.reason);
          }
        }
        closing = true;
        pendingEndPromise = PromisePrototypeThen(
          cleanup(), () => totalBytesWritten);
        return pendingEndPromise;
      },

      endSync() {
        if (error) return -1;
        if (closed) return totalBytesWritten;
        if (asyncPending) return -1;
        closed = true;
        handle[kLocked] = false;
        handle[kUnref]();
        if (autoClose) {
          handle[kCloseSync]();
        }
        return totalBytesWritten;
      },

      fail(reason) {
        if (closed || error) return;
        error = reason ?? new ERR_INVALID_STATE('Failed');
        closed = true;
        handle[kLocked] = false;
        handle[kUnref]();
        if (autoClose) {
          handle[kCloseSync]();
        }
      },

      [SymbolAsyncDispose]() {
        if (closing) {
          return pendingEndPromise ?? PromiseResolve();
        }
        if (!closed && !error) {
          this.fail();
        }
        return PromiseResolve();
      },

      [SymbolDispose]() {
        this.fail();
      },
    };
  };
}

async function handleFdClose(fileOpPromise, closeFunc) {
  return PromisePrototypeThen(
    fileOpPromise,
    (result) => PromisePrototypeThen(closeFunc(), () => result),
    (opError) =>
      PromisePrototypeThen(
        closeFunc(),
        () => PromiseReject(opError),
        (closeError) => PromiseReject(aggregateTwoErrors(closeError, opError)),
      ),
  );
}

async function handleFdSync(fileOpPromise, handle) {
  return PromisePrototypeThen(
    fileOpPromise,
    (result) => PromisePrototypeThen(
      handle.sync(),
      () => result,
      (syncError) => PromiseReject(syncError),
    ),
    (opError) => PromiseReject(opError),
  );
}

async function fsCall(fn, handle, ...args) {
  assert(handle[kRefs] !== undefined,
         'handle must be an instance of FileHandle');

  if (handle.fd === -1) {
    // eslint-disable-next-line no-restricted-syntax
    const err = new Error(handle[kCloseReason] ?? 'file closed');
    err.code = 'EBADF';
    err.syscall = fn.name;
    throw err;
  }

  try {
    handle[kRef]();
    return await fn(handle, ...new SafeArrayIterator(args));
  } finally {
    handle[kUnref]();
  }
}

function checkAborted(signal) {
  if (signal?.aborted)
    throw new AbortError(undefined, { cause: signal.reason });
}

async function writeFileHandle(filehandle, data, signal, encoding) {
  checkAborted(signal);
  if (isCustomIterable(data)) {
    for await (const buf of data) {
      checkAborted(signal);
      const toWrite =
        isArrayBufferView(buf) ? buf : Buffer.from(buf, encoding || 'utf8');
      let remaining = toWrite.byteLength;
      while (remaining > 0) {
        const writeSize = MathMin(kWriteFileMaxChunkSize, remaining);
        const { bytesWritten } = await write(
          filehandle, toWrite, toWrite.byteLength - remaining, writeSize);
        remaining -= bytesWritten;
        checkAborted(signal);
      }
    }
    return;
  }
  data = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
  let remaining = data.byteLength;
  if (remaining === 0) return;
  do {
    checkAborted(signal);
    const { bytesWritten } =
      await write(filehandle, data, 0,
                  MathMin(kWriteFileMaxChunkSize, data.byteLength));
    remaining -= bytesWritten;
    data = new Uint8Array(
      data.buffer,
      data.byteOffset + bytesWritten,
      data.byteLength - bytesWritten,
    );
  } while (remaining > 0);
}

async function readFileHandle(filehandle, options) {
  const signal = options?.signal;
  const encoding = options?.encoding;
  const decoder = encoding && new StringDecoder(encoding);

  checkAborted(signal);

  const statFields = await PromisePrototypeThen(
    binding.fstat(filehandle.fd, false, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );

  checkAborted(signal);

  let size = 0;
  let length = 0;
  if ((statFields[1/* mode */] & S_IFMT) === S_IFREG) {
    size = statFields[8/* size */];
    length = encoding ? MathMin(size, kReadFileBufferLength) : size;
  }
  if (length === 0) {
    length = kReadFileUnknownBufferLength;
  }

  if (size > kIoMaxLength)
    throw new ERR_FS_FILE_TOO_LARGE(size);

  let totalRead = 0;
  const noSize = size === 0;
  let buffer = Buffer.allocUnsafeSlow(length);
  let result = '';
  let offset = 0;
  let buffers;
  const chunkedRead = length > kReadFileBufferLength;

  while (true) {
    checkAborted(signal);

    if (chunkedRead) {
      length = MathMin(size - totalRead, kReadFileBufferLength);
    }

    const bytesRead = (await PromisePrototypeThen(
      binding.read(filehandle.fd, buffer, offset, length, -1, kUsePromises),
      undefined,
      handleErrorFromBinding,
    )) ?? 0;
    totalRead += bytesRead;

    if (bytesRead === 0 ||
        totalRead === size ||
        (bytesRead !== buffer.length && !chunkedRead && !noSize)) {
      const singleRead = bytesRead === totalRead;

      const bytesToCheck = chunkedRead ? totalRead : bytesRead;

      if (bytesToCheck !== buffer.length) {
        buffer = buffer.subarray(0, bytesToCheck);
      }

      if (!encoding) {
        if (noSize && !singleRead) {
          ArrayPrototypePush(buffers, buffer);
          return Buffer.concat(buffers, totalRead);
        }
        return buffer;
      }

      if (singleRead) {
        return buffer.toString(encoding);
      }
      result += decoder.end(buffer);
      return result;
    }
    const readBuffer = bytesRead !== buffer.length ?
      buffer.subarray(0, bytesRead) :
      buffer;
    if (encoding) {
      result += decoder.write(readBuffer);
    } else if (size !== 0) {
      offset = totalRead;
    } else {
      buffers ??= [];
      // Unknown file size requires chunks.
      ArrayPrototypePush(buffers, readBuffer);
      buffer = Buffer.allocUnsafeSlow(kReadFileUnknownBufferLength);
    }
  }
}

// All of the functions are defined as async in order to ensure that errors
// thrown cause promise rejections rather than being thrown synchronously.
async function access(path, mode = F_OK) {
  return await PromisePrototypeThen(
    binding.access(getValidatedPath(path), mode, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function cp(src, dest, options) {
  options = validateCpOptions(options);
  src = getValidatedPath(src, 'src');
  dest = getValidatedPath(dest, 'dest');
  return lazyLoadCpPromises()(src, dest, options);
}

async function copyFile(src, dest, mode) {
  return await PromisePrototypeThen(
    binding.copyFile(
      getValidatedPath(src, 'src'),
      getValidatedPath(dest, 'dest'),
      mode,
      kUsePromises,
    ),
    undefined,
    handleErrorFromBinding,
  );
}

// Note that unlike fs.open() which uses numeric file descriptors,
// fsPromises.open() uses the fs.FileHandle class.
async function open(path, flags, mode) {
  path = getValidatedPath(path);
  const flagsNumber = stringToFlags(flags);
  mode = parseFileMode(mode, 'mode', 0o666);
  return new FileHandle(await PromisePrototypeThen(
    binding.openFileHandle(path, flagsNumber, mode, kUsePromises),
    undefined,
    handleErrorFromBinding,
  ));
}

async function read(handle, bufferOrParams, offset, length, position) {
  let buffer = bufferOrParams;
  if (!isArrayBufferView(buffer)) {
    // This is fh.read(params)
    if (bufferOrParams !== undefined) {
      validateObject(bufferOrParams, 'options', kValidateObjectAllowNullable);
    }
    ({
      buffer = Buffer.alloc(16384),
      offset = 0,
      length = buffer.byteLength - offset,
      position = null,
    } = bufferOrParams ?? kEmptyObject);

    validateBuffer(buffer);
  }

  if (offset !== null && typeof offset === 'object') {
    // This is fh.read(buffer, options)
    ({
      offset = 0,
      length = buffer.byteLength - offset,
      position = null,
    } = offset);
  }

  if (offset == null) {
    offset = 0;
  } else {
    validateInteger(offset, 'offset', 0);
  }

  length ??= buffer.byteLength - offset;

  if (position == null) {
    position = -1;
  } else {
    validatePosition(position, 'position', length);
  }

  if (length === 0)
    return { __proto__: null, bytesRead: length, buffer };

  if (buffer.byteLength === 0) {
    throw new ERR_INVALID_ARG_VALUE('buffer', buffer,
                                    'is empty and cannot be written');
  }

  validateOffsetLengthRead(offset, length, buffer.byteLength);

  const bytesRead = (await PromisePrototypeThen(
    binding.read(handle.fd, buffer, offset, length, position, kUsePromises),
    undefined,
    handleErrorFromBinding,
  )) || 0;

  return { __proto__: null, bytesRead, buffer };
}

async function readv(handle, buffers, position) {
  validateBufferArray(buffers);

  if (typeof position !== 'number')
    position = null;

  const bytesRead = (await PromisePrototypeThen(
    binding.readBuffers(handle.fd, buffers, position, kUsePromises),
    undefined,
    handleErrorFromBinding,
  )) || 0;
  return { __proto__: null, bytesRead, buffers };
}

async function write(handle, buffer, offsetOrOptions, length, position) {
  if (buffer?.byteLength === 0)
    return { __proto__: null, bytesWritten: 0, buffer };

  let offset = offsetOrOptions;
  if (isArrayBufferView(buffer)) {
    if (typeof offset === 'object') {
      ({
        offset = 0,
        length = buffer.byteLength - offset,
        position = null,
      } = offsetOrOptions ?? kEmptyObject);
    }

    if (offset == null) {
      offset = 0;
    } else {
      validateInteger(offset, 'offset', 0);
    }
    if (typeof length !== 'number')
      length = buffer.byteLength - offset;
    if (typeof position !== 'number')
      position = null;
    validateOffsetLengthWrite(offset, length, buffer.byteLength);
    const bytesWritten =
      (await PromisePrototypeThen(
        binding.writeBuffer(handle.fd, buffer, offset,
                            length, position, kUsePromises),
        undefined,
        handleErrorFromBinding,
      )) || 0;
    return { __proto__: null, bytesWritten, buffer };
  }

  validateStringAfterArrayBufferView(buffer, 'buffer');
  validateEncoding(buffer, length);
  const bytesWritten = (await PromisePrototypeThen(
    binding.writeString(handle.fd, buffer, offset, length, kUsePromises),
    undefined,
    handleErrorFromBinding,
  )) || 0;
  return { __proto__: null, bytesWritten, buffer };
}

async function writev(handle, buffers, position) {
  validateBufferArray(buffers);

  if (typeof position !== 'number')
    position = null;

  if (buffers.length === 0) {
    return { __proto__: null, bytesWritten: 0, buffers };
  }

  const bytesWritten = (await PromisePrototypeThen(
    binding.writeBuffers(handle.fd, buffers, position, kUsePromises),
    undefined,
    handleErrorFromBinding,
  )) || 0;
  return { __proto__: null, bytesWritten, buffers };
}

async function rename(oldPath, newPath) {
  oldPath = getValidatedPath(oldPath, 'oldPath');
  newPath = getValidatedPath(newPath, 'newPath');
  return await PromisePrototypeThen(
    binding.rename(oldPath, newPath, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function truncate(path, len = 0) {
  const fd = await open(path, 'r+');
  return handleFdClose(ftruncate(fd, len), fd.close);
}

async function ftruncate(handle, len = 0) {
  validateInteger(len, 'len');
  len = MathMax(0, len);
  return await PromisePrototypeThen(
    binding.ftruncate(handle.fd, len, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function rm(path, options) {
  path = getValidatedPath(path);
  options = await validateRmOptionsPromise(path, options, false);
  return lazyRimRaf()(path, options);
}

async function rmdir(path, options) {
  path = getValidatedPath(path);

  if (options?.recursive !== undefined) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.recursive',
      options.recursive,
      'is no longer supported',
    );
  }

  options = validateRmdirOptions(options);

  return await PromisePrototypeThen(
    binding.rmdir(path, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function fdatasync(handle) {
  return await PromisePrototypeThen(
    binding.fdatasync(handle.fd, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function fsync(handle) {
  return await PromisePrototypeThen(
    binding.fsync(handle.fd, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function mkdir(path, options) {
  if (typeof options === 'number' || typeof options === 'string') {
    options = { mode: options };
  }
  const {
    recursive = false,
    mode = 0o777,
  } = options || kEmptyObject;
  path = getValidatedPath(path);
  validateBoolean(recursive, 'options.recursive');

  return await PromisePrototypeThen(
    binding.mkdir(
      path,
      parseFileMode(mode, 'mode', 0o777),
      recursive,
      kUsePromises,
    ),
    undefined,
    handleErrorFromBinding,
  );
}

async function readdirRecursive(originalPath, options) {
  const result = [];
  const queue = [
    [
      originalPath,
      await PromisePrototypeThen(
        binding.readdir(
          originalPath,
          options.encoding,
          !!options.withFileTypes,
          kUsePromises,
        ),
        undefined,
        handleErrorFromBinding,
      ),
    ],
  ];


  if (options.withFileTypes) {
    while (queue.length > 0) {
      // If we want to implement BFS make this a `shift` call instead of `pop`
      const { 0: path, 1: readdir } = ArrayPrototypePop(queue);
      for (const dirent of getDirents(path, readdir)) {
        ArrayPrototypePush(result, dirent);
        if (dirent.isDirectory()) {
          const direntPath = pathModule.join(path, dirent.name);
          ArrayPrototypePush(queue, [
            direntPath,
            await PromisePrototypeThen(
              binding.readdir(
                direntPath,
                options.encoding,
                true,
                kUsePromises,
              ),
              undefined,
              handleErrorFromBinding,
            ),
          ]);
        }
      }
    }
  } else {
    while (queue.length > 0) {
      const { 0: path, 1: readdir } = ArrayPrototypePop(queue);
      for (const ent of readdir) {
        const direntPath = pathModule.join(path, ent);
        const stat = binding.internalModuleStat(direntPath);
        ArrayPrototypePush(
          result,
          pathModule.relative(originalPath, direntPath),
        );
        if (stat === 1) {
          ArrayPrototypePush(queue, [
            direntPath,
            await PromisePrototypeThen(
              binding.readdir(
                direntPath,
                options.encoding,
                false,
                kUsePromises,
              ),
              undefined,
              handleErrorFromBinding,
            ),
          ]);
        }
      }
    }
  }

  return result;
}

async function readdir(path, options) {
  options = getOptions(options);

  // Make shallow copy to prevent mutating options from affecting results
  options = copyObject(options);

  path = getValidatedPath(path);
  if (options.recursive) {
    return readdirRecursive(path, options);
  }
  const result = await PromisePrototypeThen(
    binding.readdir(
      path,
      options.encoding,
      !!options.withFileTypes,
      kUsePromises,
    ),
    undefined,
    handleErrorFromBinding,
  );
  return options.withFileTypes ?
    getDirectoryEntriesPromise(path, result) :
    result;
}

async function readlink(path, options) {
  options = getOptions(options);
  path = getValidatedPath(path, 'oldPath');
  return await PromisePrototypeThen(
    binding.readlink(path, options.encoding, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function symlink(target, path, type) {
  validateOneOf(type, 'type', ['dir', 'file', 'junction', null, undefined]);
  if (isWindows && type == null) {
    try {
      const absoluteTarget = pathModule.resolve(`${path}`, '..', `${target}`);
      type = (await stat(absoluteTarget)).isDirectory() ? 'dir' : 'file';
    } catch {
      // Default to 'file' if path is invalid or file does not exist
      type = 'file';
    }
  }

  // Due to the nature of Node.js runtime, symlinks has different edge cases that can bypass
  // the permission model security guarantees. Thus, this API is disabled unless fs.read
  // and fs.write permission has been given.
  if (permission.isEnabled() && !permission.has('fs')) {
    throw new ERR_ACCESS_DENIED('fs.symlink API requires full fs.read and fs.write permissions.');
  }

  target = getValidatedPath(target, 'target');
  path = getValidatedPath(path);
  return await PromisePrototypeThen(
    binding.symlink(
      preprocessSymlinkDestination(target, type, path),
      path,
      stringToSymlinkType(type),
      kUsePromises,
    ),
    undefined,
    handleErrorFromBinding,
  );
}

async function fstat(handle, options = { bigint: false }) {
  const result = await PromisePrototypeThen(
    binding.fstat(handle.fd, options.bigint, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
  return getStatsFromBinding(result);
}

async function lstat(path, options = { bigint: false }) {
  path = getValidatedPath(path);
  if (permission.isEnabled() && !permission.has('fs.read', path)) {
    const resource = pathModule.toNamespacedPath(BufferIsBuffer(path) ? BufferToString(path) : path);
    throw new ERR_ACCESS_DENIED('Access to this API has been restricted', 'FileSystemRead', resource);
  }
  const result = await PromisePrototypeThen(
    binding.lstat(path, options.bigint, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
  return getStatsFromBinding(result);
}

async function stat(path, options = { bigint: false, throwIfNoEntry: true }) {
  const result = await PromisePrototypeThen(
    binding.stat(getValidatedPath(path), options.bigint, kUsePromises, options.throwIfNoEntry),
    undefined,
    handleErrorFromBinding,
  );

  // Binding will resolve undefined if UV_ENOENT or UV_ENOTDIR and throwIfNoEntry is false
  if (!options.throwIfNoEntry && result === undefined) return undefined;

  return getStatsFromBinding(result);
}

async function statfs(path, options = { bigint: false }) {
  const result = await PromisePrototypeThen(
    binding.statfs(getValidatedPath(path), options.bigint, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
  return getStatFsFromBinding(result);
}

async function link(existingPath, newPath) {
  existingPath = getValidatedPath(existingPath, 'existingPath');
  newPath = getValidatedPath(newPath, 'newPath');
  return await PromisePrototypeThen(
    binding.link(existingPath, newPath, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function unlink(path) {
  return await PromisePrototypeThen(
    binding.unlink(getValidatedPath(path), kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function fchmod(handle, mode) {
  if (permission.isEnabled()) {
    throw new ERR_ACCESS_DENIED('fchmod API is disabled when Permission Model is enabled.');
  }
  mode = parseFileMode(mode, 'mode');
  return await PromisePrototypeThen(
    binding.fchmod(handle.fd, mode, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function chmod(path, mode) {
  path = getValidatedPath(path);
  mode = parseFileMode(mode, 'mode');
  return await PromisePrototypeThen(
    binding.chmod(path, mode, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function lchmod(path, mode) {
  if (O_SYMLINK === undefined)
    throw new ERR_METHOD_NOT_IMPLEMENTED('lchmod()');

  const fd = await open(path, O_WRONLY | O_SYMLINK);
  return handleFdClose(fchmod(fd, mode), fd.close);
}

async function lchown(path, uid, gid) {
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  return await PromisePrototypeThen(
    binding.lchown(path, uid, gid, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function fchown(handle, uid, gid) {
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  if (permission.isEnabled()) {
    throw new ERR_ACCESS_DENIED('fchown API is disabled when Permission Model is enabled.');
  }
  return await PromisePrototypeThen(
    binding.fchown(handle.fd, uid, gid, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function chown(path, uid, gid) {
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  return await PromisePrototypeThen(
    binding.chown(path, uid, gid, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function utimes(path, atime, mtime) {
  path = getValidatedPath(path);
  return await PromisePrototypeThen(
    binding.utimes(
      path,
      toUnixTimestamp(atime),
      toUnixTimestamp(mtime),
      kUsePromises,
    ),
    undefined,
    handleErrorFromBinding,
  );
}

async function futimes(handle, atime, mtime) {
  atime = toUnixTimestamp(atime, 'atime');
  mtime = toUnixTimestamp(mtime, 'mtime');
  return await PromisePrototypeThen(
    binding.futimes(handle.fd, atime, mtime, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function lutimes(path, atime, mtime) {
  return await PromisePrototypeThen(
    binding.lutimes(
      getValidatedPath(path),
      toUnixTimestamp(atime),
      toUnixTimestamp(mtime),
      kUsePromises,
    ),
    undefined,
    handleErrorFromBinding,
  );
}

async function realpath(path, options) {
  options = getOptions(options);
  return await PromisePrototypeThen(
    binding.realpath(getValidatedPath(path), options.encoding, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function mkdtemp(prefix, options) {
  options = getOptions(options);

  prefix = getValidatedPath(prefix, 'prefix');
  warnOnNonPortableTemplate(prefix);

  return await PromisePrototypeThen(
    binding.mkdtemp(prefix, options.encoding, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
}

async function mkdtempDisposable(prefix, options) {
  options = getOptions(options);

  prefix = getValidatedPath(prefix, 'prefix');
  warnOnNonPortableTemplate(prefix);

  const cwd = process.cwd();
  const path = await PromisePrototypeThen(
    binding.mkdtemp(prefix, options.encoding, kUsePromises),
    undefined,
    handleErrorFromBinding,
  );
  // Stash the full path in case of process.chdir()
  const fullPath = pathModule.resolve(cwd, path);

  const remove = async () => {
    const rmrf = lazyRimRaf();
    await rmrf(fullPath, {
      maxRetries: 0,
      recursive: true,
      retryDelay: 0,
    });
  };
  return {
    __proto__: null,
    path,
    remove,
    async [SymbolAsyncDispose]() {
      await remove();
    },
  };
}

async function writeFile(path, data, options) {
  options = getOptions(options, {
    encoding: 'utf8',
    mode: 0o666,
    flag: 'w',
    flush: false,
  });
  const flag = options.flag || 'w';
  const flush = options.flush ?? false;

  validateBoolean(flush, 'options.flush');

  if (!isArrayBufferView(data) && !isCustomIterable(data)) {
    validateStringAfterArrayBufferView(data, 'data');
    data = Buffer.from(data, options.encoding || 'utf8');
  }

  validateAbortSignal(options.signal);
  if (path instanceof FileHandle)
    return writeFileHandle(path, data, options.signal, options.encoding);

  checkAborted(options.signal);

  const fd = await open(path, flag, options.mode);
  let writeOp = writeFileHandle(fd, data, options.signal, options.encoding);

  if (flush) {
    writeOp = handleFdSync(writeOp, fd);
  }

  return handleFdClose(writeOp, fd.close);
}

function isCustomIterable(obj) {
  return isIterable(obj) && !isArrayBufferView(obj) && typeof obj !== 'string';
}

async function appendFile(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'a' });
  options = copyObject(options);
  options.flag ||= 'a';
  return writeFile(path, data, options);
}

async function readFile(path, options) {
  options = getOptions(options, { flag: 'r' });
  const flag = options.flag || 'r';

  if (path instanceof FileHandle)
    return readFileHandle(path, options);

  checkAborted(options.signal);

  const fd = await open(path, flag, 0o666);
  return handleFdClose(readFileHandle(fd, options), fd.close);
}

async function* _watch(filename, options = kEmptyObject) {
  validateObject(options, 'options');

  if (options.recursive != null) {
    validateBoolean(options.recursive, 'options.recursive');

    // TODO(anonrig): Remove non-native watcher when/if libuv supports recursive.
    // As of November 2022, libuv does not support recursive file watch on all platforms,
    // e.g. Linux due to the limitations of inotify.
    if (options.recursive && !isMacOS && !isWindows) {
      const watcher = new nonNativeWatcher.FSWatcher(options);
      watcher[kFSWatchStart](filename);
      yield* watcher;
      return;
    }
  }

  yield* watch(filename, options);
}

const lazyGlob = getLazy(() => require('internal/fs/glob').Glob);
async function* glob(pattern, options) {
  const Glob = lazyGlob();
  yield* new Glob(pattern, options).glob();
}

module.exports = {
  exports: {
    access,
    copyFile,
    cp,
    glob,
    open,
    opendir: promisify(opendir),
    rename,
    truncate,
    rm,
    rmdir,
    mkdir,
    readdir,
    readlink,
    symlink,
    lstat,
    stat,
    statfs,
    link,
    unlink,
    chmod,
    lchmod,
    lchown,
    chown,
    utimes,
    lutimes,
    realpath,
    mkdtemp,
    mkdtempDisposable,
    writeFile,
    appendFile,
    readFile,
    watch: !isMacOS && !isWindows ? _watch : watch,
    constants,
  },

  FileHandle,
  kRef,
  kUnref,
};
