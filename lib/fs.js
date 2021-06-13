// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// Maintainers, keep in mind that ES1-style octal literals (`0666`) are not
// allowed in strict mode. Use ES6-style octal literals instead (`0o666`).

'use strict';

// When using FSReqCallback, make sure to create the object only *after* all
// parameter validation has happened, so that the objects are not kept in memory
// in case they are created but never used due to an exception.

const {
  ArrayPrototypePush,
  BigIntPrototypeToString,
  MathMax,
  Number,
  ObjectCreate,
  ObjectDefineProperties,
  ObjectDefineProperty,
  Promise,
  ReflectApply,
  RegExpPrototypeExec,
  SafeMap,
  String,
  StringPrototypeCharCodeAt,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
} = primordials;

const { fs: constants } = internalBinding('constants');
const {
  S_IFIFO,
  S_IFLNK,
  S_IFMT,
  S_IFREG,
  S_IFSOCK,
  F_OK,
  R_OK,
  W_OK,
  X_OK,
  O_WRONLY,
  O_SYMLINK
} = constants;

const pathModule = require('path');
const { isArrayBufferView } = require('internal/util/types');

// We need to get the statValues from the binding at the callsite since
// it's re-initialized after deserialization.

const binding = internalBinding('fs');
const { Buffer } = require('buffer');
const {
  aggregateTwoErrors,
  codes: {
    ERR_FS_FILE_TOO_LARGE,
    ERR_INVALID_ARG_VALUE,
    ERR_FEATURE_UNAVAILABLE_ON_PLATFORM,
  },
  AbortError,
  uvErrmapGet,
  uvException
} = require('internal/errors');

const { FSReqCallback } = binding;
const { toPathIfFileURL } = require('internal/url');
const internalUtil = require('internal/util');
const {
  constants: {
    kIoMaxLength,
    kMaxUserId,
  },
  copyObject,
  Dirent,
  emitRecursiveRmdirWarning,
  getDirents,
  getOptions,
  getValidatedFd,
  getValidatedPath,
  getValidMode,
  handleErrorFromBinding,
  nullCheck,
  preprocessSymlinkDestination,
  Stats,
  getStatsFromBinding,
  realpathCacheKey,
  stringToFlags,
  stringToSymlinkType,
  toUnixTimestamp,
  validateBufferArray,
  validateOffsetLengthRead,
  validateOffsetLengthWrite,
  validatePath,
  validatePosition,
  validateRmOptions,
  validateRmOptionsSync,
  validateRmdirOptions,
  validateStringAfterArrayBufferView,
  warnOnNonPortableTemplate
} = require('internal/fs/utils');
const {
  Dir,
  opendir,
  opendirSync
} = require('internal/fs/dir');
const {
  CHAR_FORWARD_SLASH,
  CHAR_BACKWARD_SLASH,
} = require('internal/constants');
const {
  isUint32,
  parseFileMode,
  validateBoolean,
  validateBuffer,
  validateCallback,
  validateEncoding,
  validateFunction,
  validateInteger,
  validateString,
} = require('internal/validators');

const watchers = require('internal/fs/watchers');
const ReadFileContext = require('internal/fs/read_file_context');

let truncateWarn = true;
let fs;

// Lazy loaded
let promises = null;
let ReadStream;
let WriteStream;
let rimraf;
let rimrafSync;

// These have to be separate because of how graceful-fs happens to do it's
// monkeypatching.
let FileReadStream;
let FileWriteStream;

const isWindows = process.platform === 'win32';
const isOSX = process.platform === 'darwin';


function showTruncateDeprecation() {
  if (truncateWarn) {
    process.emitWarning(
      'Using fs.truncate with a file descriptor is deprecated. Please use ' +
      'fs.ftruncate with a file descriptor instead.',
      'DeprecationWarning', 'DEP0081');
    truncateWarn = false;
  }
}

function maybeCallback(cb) {
  validateCallback(cb);

  return cb;
}

// Ensure that callbacks run in the global context. Only use this function
// for callbacks that are passed to the binding layer, callbacks that are
// invoked from JS already run in the proper scope.
function makeCallback(cb) {
  validateCallback(cb);

  return (...args) => ReflectApply(cb, this, args);
}

// Special case of `makeCallback()` that is specific to async `*stat()` calls as
// an optimization, since the data passed back to the callback needs to be
// transformed anyway.
function makeStatsCallback(cb) {
  validateCallback(cb);

  return (err, stats) => {
    if (err) return cb(err);
    cb(err, getStatsFromBinding(stats));
  };
}

const isFd = isUint32;

function isFileType(stats, fileType) {
  // Use stats array directly to avoid creating an fs.Stats instance just for
  // our internal use.
  let mode = stats[1];
  if (typeof mode === 'bigint')
    mode = Number(mode);
  return (mode & S_IFMT) === fileType;
}

/**
 * Tests a user's permissions for the file or directory
 * specified by `path`.
 * @param {string | Buffer | URL} path
 * @param {number} [mode]
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function access(path, mode, callback) {
  if (typeof mode === 'function') {
    callback = mode;
    mode = F_OK;
  }

  path = getValidatedPath(path);
  mode = getValidMode(mode, 'access');
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.access(pathModule.toNamespacedPath(path), mode, req);
}

/**
 * Synchronously tests a user's permissions for the file or
 * directory specified by `path`.
 * @param {string | Buffer | URL} path
 * @param {number} [mode]
 * @returns {void | never}
 */
function accessSync(path, mode) {
  path = getValidatedPath(path);
  mode = getValidMode(mode, 'access');

  const ctx = { path };
  binding.access(pathModule.toNamespacedPath(path), mode, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Tests whether or not the given path exists.
 * @param {string | Buffer | URL} path
 * @param {(exists?: boolean) => any} callback
 * @returns {void}
 */
function exists(path, callback) {
  maybeCallback(callback);

  function suppressedCallback(err) {
    callback(err ? false : true);
  }

  try {
    fs.access(path, F_OK, suppressedCallback);
  } catch {
    return callback(false);
  }
}

ObjectDefineProperty(exists, internalUtil.promisify.custom, {
  value: (path) => {
    return new Promise((resolve) => fs.exists(path, resolve));
  }
});

// fs.existsSync never throws, it only returns true or false.
// Since fs.existsSync never throws, users have established
// the expectation that passing invalid arguments to it, even like
// fs.existsSync(), would only get a false in return, so we cannot signal
// validation errors to users properly out of compatibility concerns.
// TODO(joyeecheung): deprecate the never-throw-on-invalid-arguments behavior
/**
 * Synchronously tests whether or not the given path exists.
 * @param {string | Buffer | URL} path
 * @returns {boolean}
 */
function existsSync(path) {
  try {
    path = getValidatedPath(path);
  } catch {
    return false;
  }
  const ctx = { path };
  const nPath = pathModule.toNamespacedPath(path);
  binding.access(nPath, F_OK, undefined, ctx);

  // In case of an invalid symlink, `binding.access()` on win32
  // will **not** return an error and is therefore not enough.
  // Double check with `binding.stat()`.
  if (isWindows && ctx.errno === undefined) {
    binding.stat(nPath, false, undefined, ctx);
  }

  return ctx.errno === undefined;
}

function readFileAfterOpen(err, fd) {
  const context = this.context;

  if (err) {
    context.callback(err);
    return;
  }

  context.fd = fd;

  const req = new FSReqCallback();
  req.oncomplete = readFileAfterStat;
  req.context = context;
  binding.fstat(fd, false, req);
}

function readFileAfterStat(err, stats) {
  const context = this.context;

  if (err)
    return context.close(err);

  const size = context.size = isFileType(stats, S_IFREG) ? stats[8] : 0;

  if (size > kIoMaxLength) {
    err = new ERR_FS_FILE_TOO_LARGE(size);
    return context.close(err);
  }

  try {
    if (size === 0) {
      context.buffers = [];
    } else {
      context.buffer = Buffer.allocUnsafeSlow(size);
    }
  } catch (err) {
    return context.close(err);
  }
  context.read();
}

function checkAborted(signal, callback) {
  if (signal?.aborted) {
    callback(new AbortError());
    return true;
  }
  return false;
}

/**
 * Asynchronously reads the entire contents of a file.
 * @param {string | Buffer | URL | number} path
 * @param {{
 *   encoding?: string | null;
 *   flag?: string;
 *   signal?: AbortSignal;
 *   } | string} [options]
 * @param {(
 *   err?: Error,
 *   data?: string | Buffer
 *   ) => any} callback
 * @returns {void}
 */
function readFile(path, options, callback) {
  callback = maybeCallback(callback || options);
  options = getOptions(options, { flag: 'r' });
  const context = new ReadFileContext(callback, options.encoding);
  context.isUserFd = isFd(path); // File descriptor ownership

  if (options.signal) {
    context.signal = options.signal;
  }
  if (context.isUserFd) {
    process.nextTick(function tick(context) {
      ReflectApply(readFileAfterOpen, { context }, [null, path]);
    }, context);
    return;
  }

  if (checkAborted(options.signal, callback))
    return;

  const flagsNumber = stringToFlags(options.flag, 'options.flag');
  path = getValidatedPath(path);

  const req = new FSReqCallback();
  req.context = context;
  req.oncomplete = readFileAfterOpen;
  binding.open(pathModule.toNamespacedPath(path),
               flagsNumber,
               0o666,
               req);
}

function tryStatSync(fd, isUserFd) {
  const ctx = {};
  const stats = binding.fstat(fd, false, undefined, ctx);
  if (ctx.errno !== undefined && !isUserFd) {
    fs.closeSync(fd);
    throw uvException(ctx);
  }
  return stats;
}

function tryCreateBuffer(size, fd, isUserFd) {
  let threw = true;
  let buffer;
  try {
    if (size > kIoMaxLength) {
      throw new ERR_FS_FILE_TOO_LARGE(size);
    }
    buffer = Buffer.allocUnsafe(size);
    threw = false;
  } finally {
    if (threw && !isUserFd) fs.closeSync(fd);
  }
  return buffer;
}

function tryReadSync(fd, isUserFd, buffer, pos, len) {
  let threw = true;
  let bytesRead;
  try {
    bytesRead = fs.readSync(fd, buffer, pos, len);
    threw = false;
  } finally {
    if (threw && !isUserFd) fs.closeSync(fd);
  }
  return bytesRead;
}

/**
 * Synchronously reads the entire contents of a file.
 * @param {string | Buffer | URL | number} path
 * @param {{
 *   encoding?: string | null;
 *   flag?: string;
 *   }} [options]
 * @returns {string | Buffer}
 */
function readFileSync(path, options) {
  options = getOptions(options, { flag: 'r' });
  const isUserFd = isFd(path); // File descriptor ownership
  const fd = isUserFd ? path : fs.openSync(path, options.flag, 0o666);

  const stats = tryStatSync(fd, isUserFd);
  const size = isFileType(stats, S_IFREG) ? stats[8] : 0;
  let pos = 0;
  let buffer; // Single buffer with file data
  let buffers; // List for when size is unknown

  if (size === 0) {
    buffers = [];
  } else {
    buffer = tryCreateBuffer(size, fd, isUserFd);
  }

  let bytesRead;

  if (size !== 0) {
    do {
      bytesRead = tryReadSync(fd, isUserFd, buffer, pos, size - pos);
      pos += bytesRead;
    } while (bytesRead !== 0 && pos < size);
  } else {
    do {
      // The kernel lies about many files.
      // Go ahead and try to read some bytes.
      buffer = Buffer.allocUnsafe(8192);
      bytesRead = tryReadSync(fd, isUserFd, buffer, 0, 8192);
      if (bytesRead !== 0) {
        ArrayPrototypePush(buffers, buffer.slice(0, bytesRead));
      }
      pos += bytesRead;
    } while (bytesRead !== 0);
  }

  if (!isUserFd)
    fs.closeSync(fd);

  if (size === 0) {
    // Data was collected into the buffers list.
    buffer = Buffer.concat(buffers, pos);
  } else if (pos < size) {
    buffer = buffer.slice(0, pos);
  }

  if (options.encoding) buffer = buffer.toString(options.encoding);
  return buffer;
}

function defaultCloseCallback(err) {
  if (err != null) throw err;
}

/**
 * Closes the file descriptor.
 * @param {number} fd
 * @param {(err?: Error) => any} [callback]
 * @returns {void}
 */
function close(fd, callback = defaultCloseCallback) {
  fd = getValidatedFd(fd);
  if (callback !== defaultCloseCallback)
    callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.close(fd, req);
}

/**
 * Synchronously closes the file descriptor.
 * @param {number} fd
 * @returns {void}
 */
function closeSync(fd) {
  fd = getValidatedFd(fd);

  const ctx = {};
  binding.close(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Asynchronously opens a file.
 * @param {string | Buffer | URL} path
 * @param {string | number} [flags]
 * @param {string | number} [mode]
 * @param {(
 *   err?: Error,
 *   fd?: number
 *   ) => any} callback
 * @returns {void}
 */
function open(path, flags, mode, callback) {
  path = getValidatedPath(path);
  if (arguments.length < 3) {
    callback = flags;
    flags = 'r';
    mode = 0o666;
  } else if (typeof mode === 'function') {
    callback = mode;
    mode = 0o666;
  } else {
    mode = parseFileMode(mode, 'mode', 0o666);
  }
  const flagsNumber = stringToFlags(flags);
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;

  binding.open(pathModule.toNamespacedPath(path),
               flagsNumber,
               mode,
               req);
}

/**
 * Synchronously opens a file.
 * @param {string | Buffer | URL} path
 * @param {string | number} [flags]
 * @param {string | number} [mode]
 * @returns {number}
 */
function openSync(path, flags, mode) {
  path = getValidatedPath(path);
  const flagsNumber = stringToFlags(flags);
  mode = parseFileMode(mode, 'mode', 0o666);

  const ctx = { path };
  const result = binding.open(pathModule.toNamespacedPath(path),
                              flagsNumber, mode,
                              undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}

/**
 * Reads file from the specified `fd` (file descriptor).
 * @param {number} fd
 * @param {Buffer | TypedArray | DataView} buffer
 * @param {number} offset
 * @param {number} length
 * @param {number | bigint} position
 * @param {(
 *   err?: Error,
 *   bytesRead?: number,
 *   buffer?: Buffer
 *   ) => any} callback
 * @returns {void}
 */
function read(fd, buffer, offset, length, position, callback) {
  fd = getValidatedFd(fd);

  if (arguments.length <= 3) {
    // Assume fs.read(fd, options, callback)
    let options = {};
    if (arguments.length < 3) {
      // This is fs.read(fd, callback)
      // buffer will be the callback
      callback = buffer;
    } else {
      // This is fs.read(fd, {}, callback)
      // buffer will be the options object
      // offset is the callback
      options = buffer;
      callback = offset;
    }

    ({
      buffer = Buffer.alloc(16384),
      offset = 0,
      length = buffer.byteLength,
      position
    } = options);
  }

  validateBuffer(buffer);
  callback = maybeCallback(callback);

  if (offset == null) {
    offset = 0;
  } else {
    validateInteger(offset, 'offset', 0);
  }

  length |= 0;

  if (length === 0) {
    return process.nextTick(function tick() {
      callback(null, 0, buffer);
    });
  }

  if (buffer.byteLength === 0) {
    throw new ERR_INVALID_ARG_VALUE('buffer', buffer,
                                    'is empty and cannot be written');
  }

  validateOffsetLengthRead(offset, length, buffer.byteLength);

  if (position == null)
    position = -1;

  validatePosition(position, 'position');

  function wrapper(err, bytesRead) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback(err, bytesRead || 0, buffer);
  }

  const req = new FSReqCallback();
  req.oncomplete = wrapper;

  binding.read(fd, buffer, offset, length, position, req);
}

ObjectDefineProperty(read, internalUtil.customPromisifyArgs,
                     { value: ['bytesRead', 'buffer'], enumerable: false });

/**
 * Synchronously reads the file from the
 * specified `fd` (file descriptor).
 * @param {number} fd
 * @param {Buffer | TypedArray | DataView} buffer
 * @param {{
 *   offset?: number;
 *   length?: number;
 *   position?: number | bigint;
 *   }} [offset]
 * @returns {number}
 */
function readSync(fd, buffer, offset, length, position) {
  fd = getValidatedFd(fd);

  validateBuffer(buffer);

  if (arguments.length <= 3) {
    // Assume fs.read(fd, buffer, options)
    const options = offset || {};

    ({ offset = 0, length = buffer.byteLength, position } = options);
  }

  if (offset == null) {
    offset = 0;
  } else {
    validateInteger(offset, 'offset', 0);
  }

  length |= 0;

  if (length === 0) {
    return 0;
  }

  if (buffer.byteLength === 0) {
    throw new ERR_INVALID_ARG_VALUE('buffer', buffer,
                                    'is empty and cannot be written');
  }

  validateOffsetLengthRead(offset, length, buffer.byteLength);

  if (position == null)
    position = -1;

  validatePosition(position, 'position');

  const ctx = {};
  const result = binding.read(fd, buffer, offset, length, position,
                              undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}

/**
 * Reads file from the specified `fd` (file descriptor)
 * and writes to an array of `ArrayBufferView`s.
 * @param {number} fd
 * @param {ArrayBufferView[]} buffers
 * @param {number} [position]
 * @param {(
 *   err?: Error,
 *   bytesRead?: number,
 *   buffers?: ArrayBufferView[];
 *   ) => any} callback
 * @returns {void}
 */
function readv(fd, buffers, position, callback) {
  function wrapper(err, read) {
    callback(err, read || 0, buffers);
  }

  fd = getValidatedFd(fd);
  validateBufferArray(buffers);
  callback = maybeCallback(callback || position);

  const req = new FSReqCallback();
  req.oncomplete = wrapper;

  if (typeof position !== 'number')
    position = null;

  return binding.readBuffers(fd, buffers, position, req);
}

ObjectDefineProperty(readv, internalUtil.customPromisifyArgs,
                     { value: ['bytesRead', 'buffers'], enumerable: false });

/**
 * Synchronously reads file from the
 * specified `fd` (file descriptor) and writes to an array
 * of `ArrayBufferView`s.
 * @param {number} fd
 * @param {ArrayBufferView[]} buffers
 * @param {number} [position]
 * @returns {number}
 */
function readvSync(fd, buffers, position) {
  fd = getValidatedFd(fd);
  validateBufferArray(buffers);

  const ctx = {};

  if (typeof position !== 'number')
    position = null;

  const result = binding.readBuffers(fd, buffers, position, undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}

/**
 * Writes `buffer` to the specified `fd` (file descriptor).
 * @param {number} fd
 * @param {Buffer | TypedArray | DataView | string | Object} buffer
 * @param {number} [offset]
 * @param {number} [length]
 * @param {number} [position]
 * @param {(
 *   err?: Error,
 *   bytesWritten?: number;
 *   buffer?: Buffer | TypedArray | DataView
 *   ) => any} callback
 * @returns {void}
 */
function write(fd, buffer, offset, length, position, callback) {
  function wrapper(err, written) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback(err, written || 0, buffer);
  }

  fd = getValidatedFd(fd);

  if (isArrayBufferView(buffer)) {
    callback = maybeCallback(callback || position || length || offset);
    if (offset == null || typeof offset === 'function') {
      offset = 0;
    } else {
      validateInteger(offset, 'offset', 0);
    }
    if (typeof length !== 'number')
      length = buffer.byteLength - offset;
    if (typeof position !== 'number')
      position = null;
    validateOffsetLengthWrite(offset, length, buffer.byteLength);

    const req = new FSReqCallback();
    req.oncomplete = wrapper;
    return binding.writeBuffer(fd, buffer, offset, length, position, req);
  }

  validateStringAfterArrayBufferView(buffer, 'buffer');

  if (typeof position !== 'function') {
    if (typeof offset === 'function') {
      position = offset;
      offset = null;
    } else {
      position = length;
    }
    length = 'utf8';
  }

  const str = String(buffer);
  validateEncoding(str, length);
  callback = maybeCallback(position);

  const req = new FSReqCallback();
  req.oncomplete = wrapper;
  return binding.writeString(fd, str, offset, length, req);
}

ObjectDefineProperty(write, internalUtil.customPromisifyArgs,
                     { value: ['bytesWritten', 'buffer'], enumerable: false });

/**
 * Synchronously writes `buffer` to the
 * specified `fd` (file descriptor).
 * @param {number} fd
 * @param {Buffer | TypedArray | DataView | string | Object} buffer
 * @param {number} [offset]
 * @param {number} [length]
 * @param {number} [position]
 * @returns {number}
 */
function writeSync(fd, buffer, offset, length, position) {
  fd = getValidatedFd(fd);
  const ctx = {};
  let result;
  if (isArrayBufferView(buffer)) {
    if (position === undefined)
      position = null;
    if (offset == null) {
      offset = 0;
    } else {
      validateInteger(offset, 'offset', 0);
    }
    if (typeof length !== 'number')
      length = buffer.byteLength - offset;
    validateOffsetLengthWrite(offset, length, buffer.byteLength);
    result = binding.writeBuffer(fd, buffer, offset, length, position,
                                 undefined, ctx);
  } else {
    validateStringAfterArrayBufferView(buffer, 'buffer');
    validateEncoding(buffer, length);

    if (offset === undefined)
      offset = null;
    result = binding.writeString(fd, buffer, offset, length,
                                 undefined, ctx);
  }
  handleErrorFromBinding(ctx);
  return result;
}

/**
 * Writes an array of `ArrayBufferView`s to the
 * specified `fd` (file descriptor).
 * @param {number} fd
 * @param {ArrayBufferView[]} buffers
 * @param {number} [position]
 * @param {(
 *   err?: Error,
 *   bytesWritten?: number,
 *   buffers?: ArrayBufferView[]
 *   ) => any} callback
 * @returns {void}
 */
function writev(fd, buffers, position, callback) {
  function wrapper(err, written) {
    callback(err, written || 0, buffers);
  }

  fd = getValidatedFd(fd);
  validateBufferArray(buffers);
  callback = maybeCallback(callback || position);

  const req = new FSReqCallback();
  req.oncomplete = wrapper;

  if (typeof position !== 'number')
    position = null;

  return binding.writeBuffers(fd, buffers, position, req);
}

ObjectDefineProperty(writev, internalUtil.customPromisifyArgs, {
  value: ['bytesWritten', 'buffer'],
  enumerable: false
});

/**
 * Synchronously writes an array of `ArrayBufferView`s
 * to the specified `fd` (file descriptor).
 * @param {number} fd
 * @param {ArrayBufferView[]} buffers
 * @param {number} [position]
 * @returns {number}
 */
function writevSync(fd, buffers, position) {
  fd = getValidatedFd(fd);
  validateBufferArray(buffers);

  const ctx = {};

  if (typeof position !== 'number')
    position = null;

  const result = binding.writeBuffers(fd, buffers, position, undefined, ctx);

  handleErrorFromBinding(ctx);
  return result;
}

/**
 * Asynchronously renames file at `oldPath` to
 * the pathname provided as `newPath`.
 * @param {string | Buffer | URL} oldPath
 * @param {string | Buffer | URL} newPath
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function rename(oldPath, newPath, callback) {
  callback = makeCallback(callback);
  oldPath = getValidatedPath(oldPath, 'oldPath');
  newPath = getValidatedPath(newPath, 'newPath');
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.rename(pathModule.toNamespacedPath(oldPath),
                 pathModule.toNamespacedPath(newPath),
                 req);
}


/**
 * Synchronously renames file at `oldPath` to
 * the pathname provided as `newPath`.
 * @param {string | Buffer | URL} oldPath
 * @param {string | Buffer | URL} newPath
 * @returns {void}
 */
function renameSync(oldPath, newPath) {
  oldPath = getValidatedPath(oldPath, 'oldPath');
  newPath = getValidatedPath(newPath, 'newPath');
  const ctx = { path: oldPath, dest: newPath };
  binding.rename(pathModule.toNamespacedPath(oldPath),
                 pathModule.toNamespacedPath(newPath), undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Truncates the file.
 * @param {string | Buffer | URL} path
 * @param {number} [len]
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function truncate(path, len, callback) {
  if (typeof path === 'number') {
    showTruncateDeprecation();
    return fs.ftruncate(path, len, callback);
  }
  if (typeof len === 'function') {
    callback = len;
    len = 0;
  } else if (len === undefined) {
    len = 0;
  }

  validateInteger(len, 'len');
  len = MathMax(0, len);
  callback = maybeCallback(callback);
  fs.open(path, 'r+', (er, fd) => {
    if (er) return callback(er);
    const req = new FSReqCallback();
    req.oncomplete = function oncomplete(er) {
      fs.close(fd, (er2) => {
        callback(aggregateTwoErrors(er2, er));
      });
    };
    binding.ftruncate(fd, len, req);
  });
}

/**
 * Synchronously truncates the file.
 * @param {string | Buffer | URL} path
 * @param {number} [len]
 * @returns {void}
 */
function truncateSync(path, len) {
  if (typeof path === 'number') {
    // legacy
    showTruncateDeprecation();
    return fs.ftruncateSync(path, len);
  }
  if (len === undefined) {
    len = 0;
  }
  // Allow error to be thrown, but still close fd.
  const fd = fs.openSync(path, 'r+');
  let ret;

  try {
    ret = fs.ftruncateSync(fd, len);
  } finally {
    fs.closeSync(fd);
  }
  return ret;
}

/**
 * Truncates the file descriptor.
 * @param {number} fd
 * @param {number} [len]
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function ftruncate(fd, len = 0, callback) {
  if (typeof len === 'function') {
    callback = len;
    len = 0;
  }
  fd = getValidatedFd(fd);
  validateInteger(len, 'len');
  len = MathMax(0, len);
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.ftruncate(fd, len, req);
}

/**
 * Synchronously truncates the file descriptor.
 * @param {number} fd
 * @param {number} [len]
 * @returns {void}
 */
function ftruncateSync(fd, len = 0) {
  fd = getValidatedFd(fd);
  validateInteger(len, 'len');
  len = MathMax(0, len);
  const ctx = {};
  binding.ftruncate(fd, len, undefined, ctx);
  handleErrorFromBinding(ctx);
}


function lazyLoadRimraf() {
  if (rimraf === undefined)
    ({ rimraf, rimrafSync } = require('internal/fs/rimraf'));
}

/**
 * Asynchronously removes a directory.
 * @param {string | Buffer | URL} path
 * @param {{
 *   maxRetries?: number;
 *   recursive?: boolean;
 *   retryDelay?: number;
 *   }} [options]
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function rmdir(path, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = undefined;
  }

  callback = makeCallback(callback);
  path = pathModule.toNamespacedPath(getValidatedPath(path));

  if (options?.recursive) {
    emitRecursiveRmdirWarning();
    validateRmOptions(
      path,
      { ...options, force: false },
      true,
      (err, options) => {
        if (err === false) {
          const req = new FSReqCallback();
          req.oncomplete = callback;
          return binding.rmdir(path, req);
        }
        if (err) {
          return callback(err);
        }

        lazyLoadRimraf();
        rimraf(path, options, callback);
      });
  } else {
    validateRmdirOptions(options);
    const req = new FSReqCallback();
    req.oncomplete = callback;
    return binding.rmdir(path, req);
  }
}

/**
 * Synchronously removes a directory.
 * @param {string | Buffer | URL} path
 * @param {{
 *   maxRetries?: number;
 *   recursive?: boolean;
 *   retryDelay?: number;
 *   }} [options]
 * @returns {void}
 */
function rmdirSync(path, options) {
  path = getValidatedPath(path);

  if (options?.recursive) {
    emitRecursiveRmdirWarning();
    options = validateRmOptionsSync(path, { ...options, force: false }, true);
    if (options !== false) {
      lazyLoadRimraf();
      return rimrafSync(pathModule.toNamespacedPath(path), options);
    }
  } else {
    validateRmdirOptions(options);
  }

  const ctx = { path };
  binding.rmdir(pathModule.toNamespacedPath(path), undefined, ctx);
  return handleErrorFromBinding(ctx);
}

/**
 * Asynchronously removes files and
 * directories (modeled on the standard POSIX `rm` utility).
 * @param {string | Buffer | URL} path
 * @param {{
 *   force?: boolean;
 *   maxRetries?: number;
 *   recursive?: boolean;
 *   retryDelay?: number;
 *   }} [options]
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function rm(path, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = undefined;
  }

  validateRmOptions(path, options, false, (err, options) => {
    if (err) {
      return callback(err);
    }
    lazyLoadRimraf();
    return rimraf(pathModule.toNamespacedPath(path), options, callback);
  });
}

/**
 * Synchronously removes files and
 * directories (modeled on the standard POSIX `rm` utility).
 * @param {string | Buffer | URL} path
 * @param {{
 *   force?: boolean;
 *   maxRetries?: number;
 *   recursive?: boolean;
 *   retryDelay?: number;
 *   }} [options]
 * @returns {void}
 */
function rmSync(path, options) {
  options = validateRmOptionsSync(path, options, false);

  lazyLoadRimraf();
  return rimrafSync(pathModule.toNamespacedPath(path), options);
}

/**
 * Forces all currently queued I/O operations associated
 * with the file to the operating system's synchronized
 * I/O completion state.
 * @param {number} fd
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function fdatasync(fd, callback) {
  fd = getValidatedFd(fd);
  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.fdatasync(fd, req);
}

/**
 * Synchronously forces all currently queued I/O operations
 * associated with the file to the operating
 * system's synchronized I/O completion state.
 * @param {number} fd
 * @returns {void}
 */
function fdatasyncSync(fd) {
  fd = getValidatedFd(fd);
  const ctx = {};
  binding.fdatasync(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Requests for all data for the open file descriptor
 * to be flushed to the storage device.
 * @param {number} fd
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function fsync(fd, callback) {
  fd = getValidatedFd(fd);
  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.fsync(fd, req);
}

/**
 * Synchronously requests for all data for the open
 * file descriptor to be flushed to the storage device.
 * @param {number} fd
 * @returns {void}
 */
function fsyncSync(fd) {
  fd = getValidatedFd(fd);
  const ctx = {};
  binding.fsync(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Asynchronously creates a directory.
 * @param {string | Buffer | URL} path
 * @param {{
 *   recursive?: boolean;
 *   mode?: string | number;
 *   } | number} [options]
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function mkdir(path, options, callback) {
  let mode = 0o777;
  let recursive = false;
  if (typeof options === 'function') {
    callback = options;
  } else if (typeof options === 'number' || typeof options === 'string') {
    mode = options;
  } else if (options) {
    if (options.recursive !== undefined)
      recursive = options.recursive;
    if (options.mode !== undefined)
      mode = options.mode;
  }
  callback = makeCallback(callback);
  path = getValidatedPath(path);

  validateBoolean(recursive, 'options.recursive');

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.mkdir(pathModule.toNamespacedPath(path),
                parseFileMode(mode, 'mode'), recursive, req);
}

/**
 * Synchronously creates a directory.
 * @param {string | Buffer | URL} path
 * @param {{
 *   recursive?: boolean;
 *   mode?: string | number;
 *   } | number} [options]
 * @returns {string | void}
 */
function mkdirSync(path, options) {
  let mode = 0o777;
  let recursive = false;
  if (typeof options === 'number' || typeof options === 'string') {
    mode = options;
  } else if (options) {
    if (options.recursive !== undefined)
      recursive = options.recursive;
    if (options.mode !== undefined)
      mode = options.mode;
  }
  path = getValidatedPath(path);
  validateBoolean(recursive, 'options.recursive');

  const ctx = { path };
  const result = binding.mkdir(pathModule.toNamespacedPath(path),
                               parseFileMode(mode, 'mode'), recursive,
                               undefined, ctx);
  handleErrorFromBinding(ctx);
  if (recursive) {
    return result;
  }
}

/**
 * Reads the contents of a directory.
 * @param {string | Buffer | URL} path
 * @param {string | {
 *   encoding?: string;
 *   withFileTypes?: boolean;
 *   }} [options]
 * @param {(
 *   err?: Error,
 *   files?: string[] | Buffer[] | Direct[];
 *   ) => any} callback
 * @returns {void}
 */
function readdir(path, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  path = getValidatedPath(path);

  const req = new FSReqCallback();
  if (!options.withFileTypes) {
    req.oncomplete = callback;
  } else {
    req.oncomplete = (err, result) => {
      if (err) {
        callback(err);
        return;
      }
      getDirents(path, result, callback);
    };
  }
  binding.readdir(pathModule.toNamespacedPath(path), options.encoding,
                  !!options.withFileTypes, req);
}

/**
 * Synchronously reads the contents of a directory.
 * @param {string | Buffer | URL} path
 * @param {string | {
 *   encoding?: string;
 *   withFileTypes?: boolean;
 *   }} [options]
 * @returns {string | Buffer[] | Dirent[]}
 */
function readdirSync(path, options) {
  options = getOptions(options, {});
  path = getValidatedPath(path);
  const ctx = { path };
  const result = binding.readdir(pathModule.toNamespacedPath(path),
                                 options.encoding, !!options.withFileTypes,
                                 undefined, ctx);
  handleErrorFromBinding(ctx);
  return options.withFileTypes ? getDirents(path, result) : result;
}

/**
 * Invokes the callback with the `fs.Stats`
 * for the file descriptor.
 * @param {number} fd
 * @param {{ bigint?: boolean; }} [options]
 * @param {(
 *   err?: Error,
 *   stats?: Stats
 *   ) => any} callback
 * @returns {void}
 */
function fstat(fd, options = { bigint: false }, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = {};
  }
  fd = getValidatedFd(fd);
  callback = makeStatsCallback(callback);

  const req = new FSReqCallback(options.bigint);
  req.oncomplete = callback;
  binding.fstat(fd, options.bigint, req);
}

/**
 * Retrieves the `fs.Stats` for the symbolic link
 * referred to by the `path`.
 * @param {string | Buffer | URL} path
 * @param {{ bigint?: boolean; }} [options]
 * @param {(
 *   err?: Error,
 *   stats?: Stats
 *   ) => any} callback
 * @returns {void}
 */
function lstat(path, options = { bigint: false }, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = {};
  }
  callback = makeStatsCallback(callback);
  path = getValidatedPath(path);

  const req = new FSReqCallback(options.bigint);
  req.oncomplete = callback;
  binding.lstat(pathModule.toNamespacedPath(path), options.bigint, req);
}

/**
 * Asynchronously gets the stats of a file.
 * @param {string | Buffer | URL} path
 * @param {{ bigint?: boolean; }} [options]
 * @param {(
 *   err?: Error,
 *   stats?: Stats
 *   ) => any} callback
 * @returns {void}
 */
function stat(path, options = { bigint: false }, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = {};
  }
  callback = makeStatsCallback(callback);
  path = getValidatedPath(path);

  const req = new FSReqCallback(options.bigint);
  req.oncomplete = callback;
  binding.stat(pathModule.toNamespacedPath(path), options.bigint, req);
}

function hasNoEntryError(ctx) {
  if (ctx.errno) {
    const uvErr = uvErrmapGet(ctx.errno);
    return uvErr?.[0] === 'ENOENT';
  }

  if (ctx.error) {
    return ctx.error.code === 'ENOENT';
  }

  return false;
}

/**
 * Synchronously retrieves the `fs.Stats` for
 * the file descriptor.
 * @param {number} fd
 * @param {{
 *   bigint?: boolean;
 *   throwIfNoEntry?: boolean;
 *   }} [options]
 * @returns {Stats}
 */
function fstatSync(fd, options = { bigint: false, throwIfNoEntry: true }) {
  fd = getValidatedFd(fd);
  const ctx = { fd };
  const stats = binding.fstat(fd, options.bigint, undefined, ctx);
  handleErrorFromBinding(ctx);
  return getStatsFromBinding(stats);
}

/**
 * Synchronously retrieves the `fs.Stats` for
 * the symbolic link referred to by the `path`.
 * @param {string | Buffer | URL} path
 * @param {{
 *   bigint?: boolean;
 *   throwIfNoEntry?: boolean;
 *   }} [options]
 * @returns {Stats}
 */
function lstatSync(path, options = { bigint: false, throwIfNoEntry: true }) {
  path = getValidatedPath(path);
  const ctx = { path };
  const stats = binding.lstat(pathModule.toNamespacedPath(path),
                              options.bigint, undefined, ctx);
  if (options.throwIfNoEntry === false && hasNoEntryError(ctx)) {
    return undefined;
  }
  handleErrorFromBinding(ctx);
  return getStatsFromBinding(stats);
}

/**
 * Synchronously retrieves the `fs.Stats`
 * for the `path`.
 * @param {string | Buffer | URL} path
 * @param {{
 *   bigint?: boolean;
 *   throwIfNoEntry?: boolean;
 *   }} [options]
 * @returns {Stats}
 */
function statSync(path, options = { bigint: false, throwIfNoEntry: true }) {
  path = getValidatedPath(path);
  const ctx = { path };
  const stats = binding.stat(pathModule.toNamespacedPath(path),
                             options.bigint, undefined, ctx);
  if (options.throwIfNoEntry === false && hasNoEntryError(ctx)) {
    return undefined;
  }
  handleErrorFromBinding(ctx);
  return getStatsFromBinding(stats);
}

/**
 * Reads the contents of a symbolic link
 * referred to by `path`.
 * @param {string | Buffer | URL} path
 * @param {{ encoding?: string; } | string} [options]
 * @param {(
 *   err?: Error,
 *   linkString?: string | Buffer
 *   ) => any} callback
 * @returns {void}
 */
function readlink(path, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  path = getValidatedPath(path, 'oldPath');
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.readlink(pathModule.toNamespacedPath(path), options.encoding, req);
}

/**
 * Synchronously reads the contents of a symbolic link
 * referred to by `path`.
 * @param {string | Buffer | URL} path
 * @param {{ encoding?: string; } | string} [options]
 * @returns {string | Buffer}
 */
function readlinkSync(path, options) {
  options = getOptions(options, {});
  path = getValidatedPath(path, 'oldPath');
  const ctx = { path };
  const result = binding.readlink(pathModule.toNamespacedPath(path),
                                  options.encoding, undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}

/**
 * Creates the link called `path` pointing to `target`.
 * @param {string | Buffer | URL} target
 * @param {string | Buffer | URL} path
 * @param {string} [type_]
 * @param {(err?: Error) => any} callback_
 * @returns {void}
 */
function symlink(target, path, type_, callback_) {
  const type = (typeof type_ === 'string' ? type_ : null);
  const callback = makeCallback(arguments[arguments.length - 1]);

  target = getValidatedPath(target, 'target');
  path = getValidatedPath(path);

  if (isWindows && type === null) {
    let absoluteTarget;
    try {
      // Symlinks targets can be relative to the newly created path.
      // Calculate absolute file name of the symlink target, and check
      // if it is a directory. Ignore resolve error to keep symlink
      // errors consistent between platforms if invalid path is
      // provided.
      absoluteTarget = pathModule.resolve(path, '..', target);
    } catch { }
    if (absoluteTarget !== undefined) {
      stat(absoluteTarget, (err, stat) => {
        const resolvedType = !err && stat.isDirectory() ? 'dir' : 'file';
        const resolvedFlags = stringToSymlinkType(resolvedType);
        const destination = preprocessSymlinkDestination(target,
                                                         resolvedType,
                                                         path);

        const req = new FSReqCallback();
        req.oncomplete = callback;
        binding.symlink(destination,
                        pathModule.toNamespacedPath(path), resolvedFlags, req);
      });
      return;
    }
  }

  const destination = preprocessSymlinkDestination(target, type, path);

  const flags = stringToSymlinkType(type);
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.symlink(destination, pathModule.toNamespacedPath(path), flags, req);
}

/**
 * Synchronously creates the link called `path`
 * pointing to `target`.
 * @param {string | Buffer | URL} target
 * @param {string | Buffer | URL} path
 * @param {string} [type]
 * @returns {void}
 */
function symlinkSync(target, path, type) {
  type = (typeof type === 'string' ? type : null);
  if (isWindows && type === null) {
    const absoluteTarget = pathModule.resolve(`${path}`, '..', `${target}`);
    if (statSync(absoluteTarget, { throwIfNoEntry: false })?.isDirectory()) {
      type = 'dir';
    }
  }
  target = getValidatedPath(target, 'target');
  path = getValidatedPath(path);
  const flags = stringToSymlinkType(type);

  const ctx = { path: target, dest: path };
  binding.symlink(preprocessSymlinkDestination(target, type, path),
                  pathModule.toNamespacedPath(path), flags, undefined, ctx);

  handleErrorFromBinding(ctx);
}

/**
 * Creates a new link from the `existingPath`
 * to the `newPath`.
 * @param {string | Buffer | URL} existingPath
 * @param {string | Buffer | URL} newPath
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function link(existingPath, newPath, callback) {
  callback = makeCallback(callback);

  existingPath = getValidatedPath(existingPath, 'existingPath');
  newPath = getValidatedPath(newPath, 'newPath');

  const req = new FSReqCallback();
  req.oncomplete = callback;

  binding.link(pathModule.toNamespacedPath(existingPath),
               pathModule.toNamespacedPath(newPath),
               req);
}

/**
 * Synchronously creates a new link from the `existingPath`
 * to the `newPath`.
 * @param {string | Buffer | URL} existingPath
 * @param {string | Buffer | URL} newPath
 * @returns {void}
 */
function linkSync(existingPath, newPath) {
  existingPath = getValidatedPath(existingPath, 'existingPath');
  newPath = getValidatedPath(newPath, 'newPath');

  const ctx = { path: existingPath, dest: newPath };
  const result = binding.link(pathModule.toNamespacedPath(existingPath),
                              pathModule.toNamespacedPath(newPath),
                              undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}

/**
 * Asynchronously removes a file or symbolic link.
 * @param {string | Buffer | URL} path
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function unlink(path, callback) {
  callback = makeCallback(callback);
  path = getValidatedPath(path);
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.unlink(pathModule.toNamespacedPath(path), req);
}

/**
 * Synchronously removes a file or symbolic link.
 * @param {string | Buffer | URL} path
 * @returns {void}
 */
function unlinkSync(path) {
  path = getValidatedPath(path);
  const ctx = { path };
  binding.unlink(pathModule.toNamespacedPath(path), undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Sets the permissions on the file.
 * @param {number} fd
 * @param {string | number} mode
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function fchmod(fd, mode, callback) {
  fd = getValidatedFd(fd);
  mode = parseFileMode(mode, 'mode');
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.fchmod(fd, mode, req);
}

/**
 * Synchronously sets the permissions on the file.
 * @param {number} fd
 * @param {string | number} mode
 * @returns {void}
 */
function fchmodSync(fd, mode) {
  fd = getValidatedFd(fd);
  mode = parseFileMode(mode, 'mode');
  const ctx = {};
  binding.fchmod(fd, mode, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Changes the permissions on a symbolic link.
 * @param {string | Buffer | URL} path
 * @param {number} mode
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function lchmod(path, mode, callback) {
  callback = maybeCallback(callback);
  mode = parseFileMode(mode, 'mode');
  fs.open(path, O_WRONLY | O_SYMLINK, (err, fd) => {
    if (err) {
      callback(err);
      return;
    }
    // Prefer to return the chmod error, if one occurs,
    // but still try to close, and report closing errors if they occur.
    fs.fchmod(fd, mode, (err) => {
      fs.close(fd, (err2) => {
        callback(aggregateTwoErrors(err2, err));
      });
    });
  });
}

/**
 * Synchronously changes the permissions on a symbolic link.
 * @param {string | Buffer | URL} path
 * @param {number} mode
 * @returns {void}
 */
function lchmodSync(path, mode) {
  const fd = fs.openSync(path, O_WRONLY | O_SYMLINK);

  // Prefer to return the chmod error, if one occurs,
  // but still try to close, and report closing errors if they occur.
  let ret;
  try {
    ret = fs.fchmodSync(fd, mode);
  } finally {
    fs.closeSync(fd);
  }
  return ret;
}

/**
 * Asynchronously changes the permissions of a file.
 * @param {string | Buffer | URL} path
 * @param {string | number} mode
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function chmod(path, mode, callback) {
  path = getValidatedPath(path);
  mode = parseFileMode(mode, 'mode');
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.chmod(pathModule.toNamespacedPath(path), mode, req);
}

/**
 * Synchronously changes the permissions of a file.
 * @param {string | Buffer | URL} path
 * @param {string | number} mode
 * @returns {void}
 */
function chmodSync(path, mode) {
  path = getValidatedPath(path);
  mode = parseFileMode(mode, 'mode');

  const ctx = { path };
  binding.chmod(pathModule.toNamespacedPath(path), mode, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Sets the owner of the symbolic link.
 * @param {string | Buffer | URL} path
 * @param {number} uid
 * @param {number} gid
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function lchown(path, uid, gid, callback) {
  callback = makeCallback(callback);
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.lchown(pathModule.toNamespacedPath(path), uid, gid, req);
}

/**
 * Synchronously sets the owner of the symbolic link.
 * @param {string | Buffer | URL} path
 * @param {number} uid
 * @param {number} gid
 * @returns {void}
 */
function lchownSync(path, uid, gid) {
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  const ctx = { path };
  binding.lchown(pathModule.toNamespacedPath(path), uid, gid, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Sets the owner of the file.
 * @param {number} fd
 * @param {number} uid
 * @param {number} gid
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function fchown(fd, uid, gid, callback) {
  fd = getValidatedFd(fd);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.fchown(fd, uid, gid, req);
}

/**
 * Synchronously sets the owner of the file.
 * @param {number} fd
 * @param {number} uid
 * @param {number} gid
 * @returns {void}
 */
function fchownSync(fd, uid, gid) {
  fd = getValidatedFd(fd);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);

  const ctx = {};
  binding.fchown(fd, uid, gid, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Asynchronously changes the owner and group
 * of a file.
 * @param {string | Buffer | URL} path
 * @param {number} uid
 * @param {number} gid
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function chown(path, uid, gid, callback) {
  callback = makeCallback(callback);
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.chown(pathModule.toNamespacedPath(path), uid, gid, req);
}

/**
 * Synchronously changes the owner and group
 * of a file.
 * @param {string | Buffer | URL} path
 * @param {number} uid
 * @param {number} gid
 * @returns {void}
 */
function chownSync(path, uid, gid) {
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  const ctx = { path };
  binding.chown(pathModule.toNamespacedPath(path), uid, gid, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Changes the file system timestamps of the object
 * referenced by `path`.
 * @param {string | Buffer | URL} path
 * @param {number | string | Date} atime
 * @param {number | string | Date} mtime
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function utimes(path, atime, mtime, callback) {
  callback = makeCallback(callback);
  path = getValidatedPath(path);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.utimes(pathModule.toNamespacedPath(path),
                 toUnixTimestamp(atime),
                 toUnixTimestamp(mtime),
                 req);
}

/**
 * Synchronously changes the file system timestamps
 * of the object referenced by `path`.
 * @param {string | Buffer | URL} path
 * @param {number | string | Date} atime
 * @param {number | string | Date} mtime
 * @returns {void}
 */
function utimesSync(path, atime, mtime) {
  path = getValidatedPath(path);
  const ctx = { path };
  binding.utimes(pathModule.toNamespacedPath(path),
                 toUnixTimestamp(atime), toUnixTimestamp(mtime),
                 undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Changes the file system timestamps of the object
 * referenced by the supplied `fd` (file descriptor).
 * @param {number} fd
 * @param {number | string | Date} atime
 * @param {number | string | Date} mtime
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function futimes(fd, atime, mtime, callback) {
  fd = getValidatedFd(fd);
  atime = toUnixTimestamp(atime, 'atime');
  mtime = toUnixTimestamp(mtime, 'mtime');
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.futimes(fd, atime, mtime, req);
}

/**
 * Synchronously changes the file system timestamps
 * of the object referenced by the
 * supplied `fd` (file descriptor).
 * @param {number} fd
 * @param {number | string | Date} atime
 * @param {number | string | Date} mtime
 * @returns {void}
 */
function futimesSync(fd, atime, mtime) {
  fd = getValidatedFd(fd);
  atime = toUnixTimestamp(atime, 'atime');
  mtime = toUnixTimestamp(mtime, 'mtime');
  const ctx = {};
  binding.futimes(fd, atime, mtime, undefined, ctx);
  handleErrorFromBinding(ctx);
}

/**
 * Changes the access and modification times of
 * a file in the same way as `fs.utimes()`.
 * @param {string | Buffer | URL} path
 * @param {number | string | Date} atime
 * @param {number | string | Date} mtime
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function lutimes(path, atime, mtime, callback) {
  callback = makeCallback(callback);
  path = getValidatedPath(path);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.lutimes(pathModule.toNamespacedPath(path),
                  toUnixTimestamp(atime),
                  toUnixTimestamp(mtime),
                  req);
}

/**
 * Synchronously changes the access and modification
 * times of a file in the same way as `fs.utimesSync()`.
 * @param {string | Buffer | URL} path
 * @param {number | string | Date} atime
 * @param {number | string | Date} mtime
 * @returns {void}
 */
function lutimesSync(path, atime, mtime) {
  path = getValidatedPath(path);
  const ctx = { path };
  binding.lutimes(pathModule.toNamespacedPath(path),
                  toUnixTimestamp(atime),
                  toUnixTimestamp(mtime),
                  undefined, ctx);
  handleErrorFromBinding(ctx);
}

function writeAll(fd, isUserFd, buffer, offset, length, signal, callback) {
  if (signal?.aborted) {
    const abortError = new AbortError();
    if (isUserFd) {
      callback(abortError);
    } else {
      fs.close(fd, (err) => {
        callback(aggregateTwoErrors(err, abortError));
      });
    }
    return;
  }
  // write(fd, buffer, offset, length, position, callback)
  fs.write(fd, buffer, offset, length, null, (writeErr, written) => {
    if (writeErr) {
      if (isUserFd) {
        callback(writeErr);
      } else {
        fs.close(fd, (err) => {
          callback(aggregateTwoErrors(err, writeErr));
        });
      }
    } else if (written === length) {
      if (isUserFd) {
        callback(null);
      } else {
        fs.close(fd, callback);
      }
    } else {
      offset += written;
      length -= written;
      writeAll(fd, isUserFd, buffer, offset, length, signal, callback);
    }
  });
}

/**
 * Asynchronously writes data to the file.
 * @param {string | Buffer | URL | number} path
 * @param {string | Buffer | TypedArray | DataView | Object} data
 * @param {{
 *   encoding?: string | null;
 *   mode?: number;
 *   flag?: string;
 *   signal?: AbortSignal;
 *   } | string} [options]
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function writeFile(path, data, options, callback) {
  callback = maybeCallback(callback || options);
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'w' });
  const flag = options.flag || 'w';

  if (!isArrayBufferView(data)) {
    validateStringAfterArrayBufferView(data, 'data');
    data = Buffer.from(String(data), options.encoding || 'utf8');
  }

  if (isFd(path)) {
    const isUserFd = true;
    const signal = options.signal;
    writeAll(path, isUserFd, data, 0, data.byteLength, signal, callback);
    return;
  }

  if (checkAborted(options.signal, callback))
    return;

  fs.open(path, flag, options.mode, (openErr, fd) => {
    if (openErr) {
      callback(openErr);
    } else {
      const isUserFd = false;
      const signal = options.signal;
      writeAll(fd, isUserFd, data, 0, data.byteLength, signal, callback);
    }
  });
}

/**
 * Synchronously writes data to the file.
 * @param {string | Buffer | URL | number} path
 * @param {string | Buffer | TypedArray | DataView | Object} data
 * @param {{
 *   encoding?: string | null;
 *   mode?: number;
 *   flag?: string;
 *   } | string} [options]
 * @returns {void}
 */
function writeFileSync(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'w' });

  if (!isArrayBufferView(data)) {
    validateStringAfterArrayBufferView(data, 'data');
    data = Buffer.from(String(data), options.encoding || 'utf8');
  }

  const flag = options.flag || 'w';

  const isUserFd = isFd(path); // File descriptor ownership
  const fd = isUserFd ? path : fs.openSync(path, flag, options.mode);

  let offset = 0;
  let length = data.byteLength;
  try {
    while (length > 0) {
      const written = fs.writeSync(fd, data, offset, length);
      offset += written;
      length -= written;
    }
  } finally {
    if (!isUserFd) fs.closeSync(fd);
  }
}

/**
 * Asynchronously appends data to a file.
 * @param {string | Buffer | URL | number} path
 * @param {string | Buffer} data
 * @param {{
 *   encoding?: string | null;
 *   mode?: number;
 *   flag?: string;
 *   } | string} [options]
 * @param {(err?: Error) => any} callback
 * @returns {void}
 */
function appendFile(path, data, options, callback) {
  callback = maybeCallback(callback || options);
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'a' });

  // Don't make changes directly on options object
  options = copyObject(options);

  // Force append behavior when using a supplied file descriptor
  if (!options.flag || isFd(path))
    options.flag = 'a';

  fs.writeFile(path, data, options, callback);
}

/**
 * Synchronously appends data to a file.
 * @param {string | Buffer | URL | number} path
 * @param {string | Buffer} data
 * @param {{
 *   encoding?: string | null;
 *   mode?: number;
 *   flag?: string;
 *   } | string} [options]
 * @returns {void}
 */
function appendFileSync(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'a' });

  // Don't make changes directly on options object
  options = copyObject(options);

  // Force append behavior when using a supplied file descriptor
  if (!options.flag || isFd(path))
    options.flag = 'a';

  fs.writeFileSync(path, data, options);
}

/**
 * Watches for the changes on `filename`.
 * @param {string | Buffer | URL} filename
 * @param {string | {
 *   persistent?: boolean;
 *   recursive?: boolean;
 *   encoding?: string;
 *   signal?: AbortSignal;
 *   }} [options]
 * @param {(
 *   eventType?: string,
 *   filename?: string | Buffer
 *   ) => any} [listener]
 * @returns {watchers.FSWatcher}
 */
function watch(filename, options, listener) {
  if (typeof options === 'function') {
    listener = options;
  }
  options = getOptions(options, {});

  // Don't make changes directly on options object
  options = copyObject(options);

  if (options.persistent === undefined) options.persistent = true;
  if (options.recursive === undefined) options.recursive = false;
  if (options.recursive && !(isOSX || isWindows))
    throw new ERR_FEATURE_UNAVAILABLE_ON_PLATFORM('watch recursively');
  const watcher = new watchers.FSWatcher();
  watcher[watchers.kFSWatchStart](filename,
                                  options.persistent,
                                  options.recursive,
                                  options.encoding);

  if (listener) {
    watcher.addListener('change', listener);
  }
  if (options.signal) {
    if (options.signal.aborted) {
      process.nextTick(() => watcher.close());
    } else {
      const listener = () => watcher.close();
      options.signal.addEventListener('abort', listener);
      watcher.once('close', () => {
        options.signal.removeEventListener('abort', listener);
      });
    }
  }

  return watcher;
}


const statWatchers = new SafeMap();

/**
 * Watches for changes on `filename`.
 * @param {string | Buffer | URL} filename
 * @param {{
 *   bigint?: boolean;
 *   persistent?: boolean;
 *   interval?: number;
 *   }} [options]
 * @param {(
 *   current?: Stats,
 *   previous?: Stats
 *   ) => any} listener
 * @returns {watchers.StatWatcher}
 */
function watchFile(filename, options, listener) {
  filename = getValidatedPath(filename);
  filename = pathModule.resolve(filename);
  let stat;

  if (options === null || typeof options !== 'object') {
    listener = options;
    options = null;
  }

  options = {
    // Poll interval in milliseconds. 5007 is what libev used to use. It's
    // a little on the slow side but let's stick with it for now to keep
    // behavioral changes to a minimum.
    interval: 5007,
    persistent: true,
    ...options
  };

  validateFunction(listener, 'listener');

  stat = statWatchers.get(filename);

  if (stat === undefined) {
    stat = new watchers.StatWatcher(options.bigint);
    stat[watchers.kFSStatWatcherStart](filename,
                                       options.persistent, options.interval);
    statWatchers.set(filename, stat);
  } else {
    stat[watchers.kFSStatWatcherAddOrCleanRef]('add');
  }

  stat.addListener('change', listener);
  return stat;
}

/**
 * Stops watching for changes on `filename`.
 * @param {string | Buffer | URL} filename
 * @param {() => any} [listener]
 * @returns {void}
 */
function unwatchFile(filename, listener) {
  filename = getValidatedPath(filename);
  filename = pathModule.resolve(filename);
  const stat = statWatchers.get(filename);

  if (stat === undefined) return;

  if (typeof listener === 'function') {
    const beforeListenerCount = stat.listenerCount('change');
    stat.removeListener('change', listener);
    if (stat.listenerCount('change') < beforeListenerCount)
      stat[watchers.kFSStatWatcherAddOrCleanRef]('clean');
  } else {
    stat.removeAllListeners('change');
    stat[watchers.kFSStatWatcherAddOrCleanRef]('cleanAll');
  }

  if (stat.listenerCount('change') === 0) {
    stat.stop();
    statWatchers.delete(filename);
  }
}


let splitRoot;
if (isWindows) {
  // Regex to find the device root on Windows (e.g. 'c:\\'), including trailing
  // slash.
  const splitRootRe = /^(?:[a-zA-Z]:|[\\/]{2}[^\\/]+[\\/][^\\/]+)?[\\/]*/;
  splitRoot = function splitRoot(str) {
    return RegExpPrototypeExec(splitRootRe, str)[0];
  };
} else {
  splitRoot = function splitRoot(str) {
    for (let i = 0; i < str.length; ++i) {
      if (StringPrototypeCharCodeAt(str, i) !== CHAR_FORWARD_SLASH)
        return StringPrototypeSlice(str, 0, i);
    }
    return str;
  };
}

function encodeRealpathResult(result, options) {
  if (!options || !options.encoding || options.encoding === 'utf8')
    return result;
  const asBuffer = Buffer.from(result);
  if (options.encoding === 'buffer') {
    return asBuffer;
  }
  return asBuffer.toString(options.encoding);
}

// Finds the next portion of a (partial) path, up to the next path delimiter
let nextPart;
if (isWindows) {
  nextPart = function nextPart(p, i) {
    for (; i < p.length; ++i) {
      const ch = StringPrototypeCharCodeAt(p, i);

      // Check for a separator character
      if (ch === CHAR_BACKWARD_SLASH || ch === CHAR_FORWARD_SLASH)
        return i;
    }
    return -1;
  };
} else {
  nextPart = function nextPart(p, i) {
    return StringPrototypeIndexOf(p, '/', i);
  };
}

const emptyObj = ObjectCreate(null);

/**
 * Returns the resolved pathname.
 * @param {string | Buffer | URL} p
 * @param {string | { encoding?: string | null; }} [options]
 * @returns {string | Buffer}
 */
function realpathSync(p, options) {
  options = getOptions(options, emptyObj);
  p = toPathIfFileURL(p);
  if (typeof p !== 'string') {
    p += '';
  }
  validatePath(p);
  p = pathModule.resolve(p);

  const cache = options[realpathCacheKey];
  const maybeCachedResult = cache?.get(p);
  if (maybeCachedResult) {
    return maybeCachedResult;
  }

  const seenLinks = ObjectCreate(null);
  const knownHard = ObjectCreate(null);
  const original = p;

  // Current character position in p
  let pos;
  // The partial path so far, including a trailing slash if any
  let current;
  // The partial path without a trailing slash (except when pointing at a root)
  let base;
  // The partial path scanned in the previous round, with slash
  let previous;

  // Skip over roots
  current = base = splitRoot(p);
  pos = current.length;

  // On windows, check that the root exists. On unix there is no need.
  if (isWindows) {
    const ctx = { path: base };
    binding.lstat(pathModule.toNamespacedPath(base), false, undefined, ctx);
    handleErrorFromBinding(ctx);
    knownHard[base] = true;
  }

  // Walk down the path, swapping out linked path parts for their real
  // values
  // NB: p.length changes.
  while (pos < p.length) {
    // find the next part
    const result = nextPart(p, pos);
    previous = current;
    if (result === -1) {
      const last = StringPrototypeSlice(p, pos);
      current += last;
      base = previous + last;
      pos = p.length;
    } else {
      current += StringPrototypeSlice(p, pos, result + 1);
      base = previous + StringPrototypeSlice(p, pos, result);
      pos = result + 1;
    }

    // Continue if not a symlink, break if a pipe/socket
    if (knownHard[base] || cache?.get(base) === base) {
      if (isFileType(binding.statValues, S_IFIFO) ||
          isFileType(binding.statValues, S_IFSOCK)) {
        break;
      }
      continue;
    }

    let resolvedLink;
    const maybeCachedResolved = cache?.get(base);
    if (maybeCachedResolved) {
      resolvedLink = maybeCachedResolved;
    } else {
      // Use stats array directly to avoid creating an fs.Stats instance just
      // for our internal use.

      const baseLong = pathModule.toNamespacedPath(base);
      const ctx = { path: base };
      const stats = binding.lstat(baseLong, true, undefined, ctx);
      handleErrorFromBinding(ctx);

      if (!isFileType(stats, S_IFLNK)) {
        knownHard[base] = true;
        cache?.set(base, base);
        continue;
      }

      // Read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      let linkTarget = null;
      let id;
      if (!isWindows) {
        const dev = BigIntPrototypeToString(stats[0], 32);
        const ino = BigIntPrototypeToString(stats[7], 32);
        id = `${dev}:${ino}`;
        if (seenLinks[id]) {
          linkTarget = seenLinks[id];
        }
      }
      if (linkTarget === null) {
        const ctx = { path: base };
        binding.stat(baseLong, false, undefined, ctx);
        handleErrorFromBinding(ctx);
        linkTarget = binding.readlink(baseLong, undefined, undefined, ctx);
        handleErrorFromBinding(ctx);
      }
      resolvedLink = pathModule.resolve(previous, linkTarget);

      if (cache) cache.set(base, resolvedLink);
      if (!isWindows) seenLinks[id] = linkTarget;
    }

    // Resolve the link, then start over
    p = pathModule.resolve(resolvedLink, StringPrototypeSlice(p, pos));

    // Skip over roots
    current = base = splitRoot(p);
    pos = current.length;

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      const ctx = { path: base };
      binding.lstat(pathModule.toNamespacedPath(base), false, undefined, ctx);
      handleErrorFromBinding(ctx);
      knownHard[base] = true;
    }
  }

  cache?.set(original, p);
  return encodeRealpathResult(p, options);
}

/**
 * Returns the resolved pathname.
 * @param {string | Buffer | URL} p
 * @param {string | { encoding?: string; }} [options]
 * @returns {string | Buffer}
 */
realpathSync.native = (path, options) => {
  options = getOptions(options, {});
  path = getValidatedPath(path);
  const ctx = { path };
  const result = binding.realpath(path, options.encoding, undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
};

/**
 * Asynchronously computes the canonical pathname by
 * resolving `.`, `..` and symbolic links.
 * @param {string | Buffer | URL} p
 * @param {string | { encoding?: string; }} [options]
 * @param {(
 *   err?: Error,
 *   resolvedPath?: string | Buffer
 *   ) => any} callback
 * @returns {void}
 */
function realpath(p, options, callback) {
  callback = typeof options === 'function' ? options : maybeCallback(callback);
  options = getOptions(options, {});
  p = toPathIfFileURL(p);

  if (typeof p !== 'string') {
    p += '';
  }
  validatePath(p);
  p = pathModule.resolve(p);

  const seenLinks = ObjectCreate(null);
  const knownHard = ObjectCreate(null);

  // Current character position in p
  let pos;
  // The partial path so far, including a trailing slash if any
  let current;
  // The partial path without a trailing slash (except when pointing at a root)
  let base;
  // The partial path scanned in the previous round, with slash
  let previous;

  current = base = splitRoot(p);
  pos = current.length;

  // On windows, check that the root exists. On unix there is no need.
  if (isWindows && !knownHard[base]) {
    fs.lstat(base, (err, stats) => {
      if (err) return callback(err);
      knownHard[base] = true;
      LOOP();
    });
  } else {
    process.nextTick(LOOP);
  }

  // Walk down the path, swapping out linked path parts for their real
  // values
  function LOOP() {
    // Stop if scanned past end of path
    if (pos >= p.length) {
      return callback(null, encodeRealpathResult(p, options));
    }

    // find the next part
    const result = nextPart(p, pos);
    previous = current;
    if (result === -1) {
      const last = StringPrototypeSlice(p, pos);
      current += last;
      base = previous + last;
      pos = p.length;
    } else {
      current += StringPrototypeSlice(p, pos, result + 1);
      base = previous + StringPrototypeSlice(p, pos, result);
      pos = result + 1;
    }

    // Continue if not a symlink, break if a pipe/socket
    if (knownHard[base]) {
      if (isFileType(binding.statValues, S_IFIFO) ||
          isFileType(binding.statValues, S_IFSOCK)) {
        return callback(null, encodeRealpathResult(p, options));
      }
      return process.nextTick(LOOP);
    }

    return fs.lstat(base, { bigint: true }, gotStat);
  }

  function gotStat(err, stats) {
    if (err) return callback(err);

    // If not a symlink, skip to the next path part
    if (!stats.isSymbolicLink()) {
      knownHard[base] = true;
      return process.nextTick(LOOP);
    }

    // Stat & read the link if not read before.
    // Call `gotTarget()` as soon as the link target is known.
    // `dev`/`ino` always return 0 on windows, so skip the check.
    let id;
    if (!isWindows) {
      const dev = BigIntPrototypeToString(stats.dev, 32);
      const ino = BigIntPrototypeToString(stats.ino, 32);
      id = `${dev}:${ino}`;
      if (seenLinks[id]) {
        return gotTarget(null, seenLinks[id]);
      }
    }
    fs.stat(base, (err) => {
      if (err) return callback(err);

      fs.readlink(base, (err, target) => {
        if (!isWindows) seenLinks[id] = target;
        gotTarget(err, target);
      });
    });
  }

  function gotTarget(err, target) {
    if (err) return callback(err);

    gotResolvedLink(pathModule.resolve(previous, target));
  }

  function gotResolvedLink(resolvedLink) {
    // Resolve the link, then start over
    p = pathModule.resolve(resolvedLink, StringPrototypeSlice(p, pos));
    current = base = splitRoot(p);
    pos = current.length;

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      fs.lstat(base, (err) => {
        if (err) return callback(err);
        knownHard[base] = true;
        LOOP();
      });
    } else {
      process.nextTick(LOOP);
    }
  }
}

/**
 * Asynchronously computes the canonical pathname by
 * resolving `.`, `..` and symbolic links.
 * @param {string | Buffer | URL} p
 * @param {string | { encoding?: string; }} [options]
 * @param {(
 *   err?: Error,
 *   resolvedPath?: string | Buffer
 *   ) => any} callback
 * @returns {void}
 */
realpath.native = (path, options, callback) => {
  callback = makeCallback(callback || options);
  options = getOptions(options, {});
  path = getValidatedPath(path);
  const req = new FSReqCallback();
  req.oncomplete = callback;
  return binding.realpath(path, options.encoding, req);
};

/**
 * Creates a unique temporary directory.
 * @param {string} prefix
 * @param {string | { encoding?: string; }} [options]
 * @param {(
 *   err?: Error,
 *   directory?: string
 *   ) => any} callback
 * @returns {void}
 */
function mkdtemp(prefix, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});

  validateString(prefix, 'prefix');
  nullCheck(prefix, 'prefix');
  warnOnNonPortableTemplate(prefix);
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.mkdtemp(`${prefix}XXXXXX`, options.encoding, req);
}

/**
 * Synchronously creates a unique temporary directory.
 * @param {string} prefix
 * @param {string | { encoding?: string; }} [options]
 * @returns {string}
 */
function mkdtempSync(prefix, options) {
  options = getOptions(options, {});

  validateString(prefix, 'prefix');
  nullCheck(prefix, 'prefix');
  warnOnNonPortableTemplate(prefix);
  const path = `${prefix}XXXXXX`;
  const ctx = { path };
  const result = binding.mkdtemp(path, options.encoding,
                                 undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}

/**
 * Asynchronously copies `src` to `dest`. By
 * default, `dest` is overwritten if it already exists.
 * @param {string | Buffer | URL} src
 * @param {string | Buffer | URL} dest
 * @param {number} [mode]
 * @param {() => any} callback
 * @returns {void}
 */
function copyFile(src, dest, mode, callback) {
  if (typeof mode === 'function') {
    callback = mode;
    mode = 0;
  }

  src = getValidatedPath(src, 'src');
  dest = getValidatedPath(dest, 'dest');

  src = pathModule._makeLong(src);
  dest = pathModule._makeLong(dest);
  mode = getValidMode(mode, 'copyFile');
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.copyFile(src, dest, mode, req);
}

/**
 * Synchronously copies `src` to `dest`. By
 * default, `dest` is overwritten if it already exists.
 * @param {string | Buffer | URL} src
 * @param {string | Buffer | URL} dest
 * @param {number} [mode]
 * @returns {void}
 */
function copyFileSync(src, dest, mode) {
  src = getValidatedPath(src, 'src');
  dest = getValidatedPath(dest, 'dest');

  const ctx = { path: src, dest };  // non-prefixed

  src = pathModule._makeLong(src);
  dest = pathModule._makeLong(dest);
  mode = getValidMode(mode, 'copyFile');
  binding.copyFile(src, dest, mode, undefined, ctx);
  handleErrorFromBinding(ctx);
}

function lazyLoadStreams() {
  if (!ReadStream) {
    ({ ReadStream, WriteStream } = require('internal/fs/streams'));
    FileReadStream = ReadStream;
    FileWriteStream = WriteStream;
  }
}

/**
 * Creates a readable stream with a default `highWaterMark`
 * of 64 kb.
 * @param {string | Buffer | URL} path
 * @param {string | {
 *   flags?: string;
 *   encoding?: string;
 *   fd?: number | FileHandle;
 *   mode?: number;
 *   autoClose?: boolean;
 *   emitClose?: boolean;
 *   start: number;
 *   end?: number;
 *   highWaterMark?: number;
 *   fs?: Object | null;
 *   }} [options]
 * @returns {ReadStream}
 */
function createReadStream(path, options) {
  lazyLoadStreams();
  return new ReadStream(path, options);
}

/**
 * Creates a write stream.
 * @param {string | Buffer | URL} path
 * @param {string | {
 *   flags?: string;
 *   encoding?: string;
 *   fd?: number | FileHandle;
 *   mode?: number;
 *   autoClose?: boolean;
 *   emitClose?: boolean;
 *   start: number;
 *   fs?: Object | null;
 *   }} [options]
 * @returns {WriteStream}
 */
function createWriteStream(path, options) {
  lazyLoadStreams();
  return new WriteStream(path, options);
}

module.exports = fs = {
  appendFile,
  appendFileSync,
  access,
  accessSync,
  chown,
  chownSync,
  chmod,
  chmodSync,
  close,
  closeSync,
  copyFile,
  copyFileSync,
  createReadStream,
  createWriteStream,
  exists,
  existsSync,
  fchown,
  fchownSync,
  fchmod,
  fchmodSync,
  fdatasync,
  fdatasyncSync,
  fstat,
  fstatSync,
  fsync,
  fsyncSync,
  ftruncate,
  ftruncateSync,
  futimes,
  futimesSync,
  lchown,
  lchownSync,
  lchmod: constants.O_SYMLINK !== undefined ? lchmod : undefined,
  lchmodSync: constants.O_SYMLINK !== undefined ? lchmodSync : undefined,
  link,
  linkSync,
  lstat,
  lstatSync,
  lutimes,
  lutimesSync,
  mkdir,
  mkdirSync,
  mkdtemp,
  mkdtempSync,
  open,
  openSync,
  opendir,
  opendirSync,
  readdir,
  readdirSync,
  read,
  readSync,
  readv,
  readvSync,
  readFile,
  readFileSync,
  readlink,
  readlinkSync,
  realpath,
  realpathSync,
  rename,
  renameSync,
  rm,
  rmSync,
  rmdir,
  rmdirSync,
  stat,
  statSync,
  symlink,
  symlinkSync,
  truncate,
  truncateSync,
  unwatchFile,
  unlink,
  unlinkSync,
  utimes,
  utimesSync,
  watch,
  watchFile,
  writeFile,
  writeFileSync,
  write,
  writeSync,
  writev,
  writevSync,
  Dir,
  Dirent,
  Stats,

  get ReadStream() {
    lazyLoadStreams();
    return ReadStream;
  },

  set ReadStream(val) {
    ReadStream = val;
  },

  get WriteStream() {
    lazyLoadStreams();
    return WriteStream;
  },

  set WriteStream(val) {
    WriteStream = val;
  },

  // Legacy names... these have to be separate because of how graceful-fs
  // (and possibly other) modules monkey patch the values.
  get FileReadStream() {
    lazyLoadStreams();
    return FileReadStream;
  },

  set FileReadStream(val) {
    FileReadStream = val;
  },

  get FileWriteStream() {
    lazyLoadStreams();
    return FileWriteStream;
  },

  set FileWriteStream(val) {
    FileWriteStream = val;
  },

  // For tests
  _toUnixTimestamp: toUnixTimestamp
};

ObjectDefineProperties(fs, {
  F_OK: { enumerable: true, value: F_OK || 0 },
  R_OK: { enumerable: true, value: R_OK || 0 },
  W_OK: { enumerable: true, value: W_OK || 0 },
  X_OK: { enumerable: true, value: X_OK || 0 },
  constants: {
    configurable: false,
    enumerable: true,
    value: constants
  },
  promises: {
    configurable: true,
    enumerable: true,
    get() {
      if (promises === null)
        promises = require('internal/fs/promises').exports;
      return promises;
    }
  }
});
