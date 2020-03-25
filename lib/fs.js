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

// Most platforms don't allow reads or writes >= 2 GB.
// See https://github.com/libuv/libuv/pull/1501.
const kIoMaxLength = 2 ** 31 - 1;

const {
  Map,
  MathMax,
  NumberIsSafeInteger,
  ObjectCreate,
  ObjectDefineProperties,
  ObjectDefineProperty,
  Promise,
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
const binding = internalBinding('fs');
const { Buffer } = require('buffer');
const {
  codes: {
    ERR_FS_FILE_TOO_LARGE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_CALLBACK,
    ERR_FEATURE_UNAVAILABLE_ON_PLATFORM
  },
  uvException
} = require('internal/errors');

const { FSReqCallback, statValues } = binding;
const { toPathIfFileURL } = require('internal/url');
const internalUtil = require('internal/util');
const {
  copyObject,
  Dirent,
  getDirents,
  getOptions,
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
  validateBuffer,
  validateInteger,
  validateInt32
} = require('internal/validators');
// 2 ** 32 - 1
const kMaxUserId = 4294967295;

let truncateWarn = true;
let fs;

// Lazy loaded
let promises = null;
let watchers;
let ReadFileContext;
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
  if (typeof cb === 'function')
    return cb;

  throw new ERR_INVALID_CALLBACK(cb);
}

// Ensure that callbacks run in the global context. Only use this function
// for callbacks that are passed to the binding layer, callbacks that are
// invoked from JS already run in the proper scope.
function makeCallback(cb) {
  if (typeof cb !== 'function') {
    throw new ERR_INVALID_CALLBACK(cb);
  }

  return (...args) => cb(...args);
}

// Special case of `makeCallback()` that is specific to async `*stat()` calls as
// an optimization, since the data passed back to the callback needs to be
// transformed anyway.
function makeStatsCallback(cb) {
  if (typeof cb !== 'function') {
    throw new ERR_INVALID_CALLBACK(cb);
  }

  return (err, stats) => {
    if (err) return cb(err);
    cb(err, getStatsFromBinding(stats));
  };
}

const isFd = isUint32;

function isFileType(stats, fileType) {
  // Use stats array directly to avoid creating an fs.Stats instance just for
  // our internal use.
  return (stats[1/* mode */] & S_IFMT) === fileType;
}

function access(path, mode, callback) {
  if (typeof mode === 'function') {
    callback = mode;
    mode = F_OK;
  }

  path = getValidatedPath(path);
  mode = getValidMode(mode, 'access');
  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.access(pathModule.toNamespacedPath(path), mode, req);
}

function accessSync(path, mode) {
  path = getValidatedPath(path);
  mode = getValidMode(mode, 'access');

  const ctx = { path };
  binding.access(pathModule.toNamespacedPath(path), mode, undefined, ctx);
  handleErrorFromBinding(ctx);
}

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

function readFile(path, options, callback) {
  callback = maybeCallback(callback || options);
  options = getOptions(options, { flag: 'r' });
  if (!ReadFileContext)
    ReadFileContext = require('internal/fs/read_file_context');
  const context = new ReadFileContext(callback, options.encoding);
  context.isUserFd = isFd(path); // File descriptor ownership

  const req = new FSReqCallback();
  req.context = context;
  req.oncomplete = readFileAfterOpen;

  if (context.isUserFd) {
    process.nextTick(function tick() {
      req.oncomplete(null, path);
    });
    return;
  }

  path = getValidatedPath(path);
  const flagsNumber = stringToFlags(options.flags);
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
        buffers.push(buffer.slice(0, bytesRead));
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

function close(fd, callback) {
  validateInt32(fd, 'fd', 0);
  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.close(fd, req);
}

function closeSync(fd) {
  validateInt32(fd, 'fd', 0);

  const ctx = {};
  binding.close(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
}

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

// usage:
// fs.read(fd, buffer, offset, length, position, callback);
// OR
// fs.read(fd, {}, callback)
function read(fd, buffer, offset, length, position, callback) {
  validateInt32(fd, 'fd', 0);

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
      length = buffer.length,
      position
    } = options);
  }

  validateBuffer(buffer);
  callback = maybeCallback(callback);

  if (offset == null) {
    offset = 0;
  } else {
    validateInteger(offset, 'offset');
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

  if (!NumberIsSafeInteger(position))
    position = -1;

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

// usage:
// fs.readSync(fd, buffer, offset, length, position);
// OR
// fs.readSync(fd, buffer, {}) or fs.readSync(fd, buffer)
function readSync(fd, buffer, offset, length, position) {
  validateInt32(fd, 'fd', 0);

  if (arguments.length <= 3) {
    // Assume fs.read(fd, buffer, options)
    const options = offset || {};

    ({ offset = 0, length = buffer.length, position } = options);
  }

  validateBuffer(buffer);

  if (offset == null) {
    offset = 0;
  } else {
    validateInteger(offset, 'offset');
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

  if (!NumberIsSafeInteger(position))
    position = -1;

  const ctx = {};
  const result = binding.read(fd, buffer, offset, length, position,
                              undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}

function readv(fd, buffers, position, callback) {
  function wrapper(err, read) {
    callback(err, read || 0, buffers);
  }

  validateInt32(fd, 'fd', /* min */ 0);
  validateBufferArray(buffers);

  const req = new FSReqCallback();
  req.oncomplete = wrapper;

  callback = maybeCallback(callback || position);

  if (typeof position !== 'number')
    position = null;

  return binding.readBuffers(fd, buffers, position, req);
}

function readvSync(fd, buffers, position) {
  validateInt32(fd, 'fd', 0);
  validateBufferArray(buffers);

  const ctx = {};

  if (typeof position !== 'number')
    position = null;

  const result = binding.readBuffers(fd, buffers, position, undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}

// usage:
//  fs.write(fd, buffer[, offset[, length[, position]]], callback);
// OR
//  fs.write(fd, string[, position[, encoding]], callback);
function write(fd, buffer, offset, length, position, callback) {
  function wrapper(err, written) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback(err, written || 0, buffer);
  }

  validateInt32(fd, 'fd', 0);

  if (isArrayBufferView(buffer)) {
    callback = maybeCallback(callback || position || length || offset);
    if (offset == null || typeof offset === 'function') {
      offset = 0;
    } else {
      validateInteger(offset, 'offset');
    }
    if (typeof length !== 'number')
      length = buffer.length - offset;
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
  callback = maybeCallback(position);

  const req = new FSReqCallback();
  req.oncomplete = wrapper;
  return binding.writeString(fd, buffer, offset, length, req);
}

ObjectDefineProperty(write, internalUtil.customPromisifyArgs,
                     { value: ['bytesWritten', 'buffer'], enumerable: false });

// Usage:
//  fs.writeSync(fd, buffer[, offset[, length[, position]]]);
// OR
//  fs.writeSync(fd, string[, position[, encoding]]);
function writeSync(fd, buffer, offset, length, position) {
  validateInt32(fd, 'fd', 0);
  const ctx = {};
  let result;
  if (isArrayBufferView(buffer)) {
    if (position === undefined)
      position = null;
    if (offset == null) {
      offset = 0;
    } else {
      validateInteger(offset, 'offset');
    }
    if (typeof length !== 'number')
      length = buffer.byteLength - offset;
    validateOffsetLengthWrite(offset, length, buffer.byteLength);
    result = binding.writeBuffer(fd, buffer, offset, length, position,
                                 undefined, ctx);
  } else {
    validateStringAfterArrayBufferView(buffer, 'buffer');

    if (offset === undefined)
      offset = null;
    result = binding.writeString(fd, buffer, offset, length,
                                 undefined, ctx);
  }
  handleErrorFromBinding(ctx);
  return result;
}

// usage:
// fs.writev(fd, buffers[, position], callback);
function writev(fd, buffers, position, callback) {
  function wrapper(err, written) {
    callback(err, written || 0, buffers);
  }

  validateInt32(fd, 'fd', 0);
  validateBufferArray(buffers);

  const req = new FSReqCallback();
  req.oncomplete = wrapper;

  callback = maybeCallback(callback || position);

  if (typeof position !== 'number')
    position = null;

  return binding.writeBuffers(fd, buffers, position, req);
}

ObjectDefineProperty(writev, internalUtil.customPromisifyArgs, {
  value: ['bytesWritten', 'buffer'],
  enumerable: false
});

function writevSync(fd, buffers, position) {
  validateInt32(fd, 'fd', 0);
  validateBufferArray(buffers);

  const ctx = {};

  if (typeof position !== 'number')
    position = null;

  const result = binding.writeBuffers(fd, buffers, position, undefined, ctx);

  handleErrorFromBinding(ctx);
  return result;
}

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

function renameSync(oldPath, newPath) {
  oldPath = getValidatedPath(oldPath, 'oldPath');
  newPath = getValidatedPath(newPath, 'newPath');
  const ctx = { path: oldPath, dest: newPath };
  binding.rename(pathModule.toNamespacedPath(oldPath),
                 pathModule.toNamespacedPath(newPath), undefined, ctx);
  handleErrorFromBinding(ctx);
}

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
  callback = maybeCallback(callback);
  fs.open(path, 'r+', (er, fd) => {
    if (er) return callback(er);
    const req = new FSReqCallback();
    req.oncomplete = function oncomplete(er) {
      fs.close(fd, (er2) => {
        callback(er || er2);
      });
    };
    binding.ftruncate(fd, len, req);
  });
}

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

function ftruncate(fd, len = 0, callback) {
  if (typeof len === 'function') {
    callback = len;
    len = 0;
  }
  validateInt32(fd, 'fd', 0);
  validateInteger(len, 'len');
  len = MathMax(0, len);
  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.ftruncate(fd, len, req);
}

function ftruncateSync(fd, len = 0) {
  validateInt32(fd, 'fd', 0);
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

function rmdir(path, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = undefined;
  }

  callback = makeCallback(callback);
  path = pathModule.toNamespacedPath(getValidatedPath(path));
  options = validateRmdirOptions(options);

  if (options.recursive) {
    lazyLoadRimraf();
    return rimraf(path, options, callback);
  }

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.rmdir(path, req);
}

function rmdirSync(path, options) {
  path = getValidatedPath(path);
  options = validateRmdirOptions(options);

  if (options.recursive) {
    lazyLoadRimraf();
    return rimrafSync(pathModule.toNamespacedPath(path), options);
  }

  const ctx = { path };
  binding.rmdir(pathModule.toNamespacedPath(path), undefined, ctx);
  handleErrorFromBinding(ctx);
}

function fdatasync(fd, callback) {
  validateInt32(fd, 'fd', 0);
  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.fdatasync(fd, req);
}

function fdatasyncSync(fd) {
  validateInt32(fd, 'fd', 0);
  const ctx = {};
  binding.fdatasync(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
}

function fsync(fd, callback) {
  validateInt32(fd, 'fd', 0);
  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.fsync(fd, req);
}

function fsyncSync(fd) {
  validateInt32(fd, 'fd', 0);
  const ctx = {};
  binding.fsync(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
}

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

  if (typeof recursive !== 'boolean')
    throw new ERR_INVALID_ARG_TYPE('options.recursive', 'boolean', recursive);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.mkdir(pathModule.toNamespacedPath(path),
                parseFileMode(mode, 'mode'), recursive, req);
}

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
  if (typeof recursive !== 'boolean')
    throw new ERR_INVALID_ARG_TYPE('options.recursive', 'boolean', recursive);

  const ctx = { path };
  const result = binding.mkdir(pathModule.toNamespacedPath(path),
                               parseFileMode(mode, 'mode'), recursive,
                               undefined, ctx);
  handleErrorFromBinding(ctx);
  if (recursive) {
    return result;
  }
}

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

function fstat(fd, options = { bigint: false }, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = {};
  }
  validateInt32(fd, 'fd', 0);
  const req = new FSReqCallback(options.bigint);
  req.oncomplete = makeStatsCallback(callback);
  binding.fstat(fd, options.bigint, req);
}

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

function fstatSync(fd, options = { bigint: false }) {
  validateInt32(fd, 'fd', 0);
  const ctx = { fd };
  const stats = binding.fstat(fd, options.bigint, undefined, ctx);
  handleErrorFromBinding(ctx);
  return getStatsFromBinding(stats);
}

function lstatSync(path, options = { bigint: false }) {
  path = getValidatedPath(path);
  const ctx = { path };
  const stats = binding.lstat(pathModule.toNamespacedPath(path),
                              options.bigint, undefined, ctx);
  handleErrorFromBinding(ctx);
  return getStatsFromBinding(stats);
}

function statSync(path, options = { bigint: false }) {
  path = getValidatedPath(path);
  const ctx = { path };
  const stats = binding.stat(pathModule.toNamespacedPath(path),
                             options.bigint, undefined, ctx);
  handleErrorFromBinding(ctx);
  return getStatsFromBinding(stats);
}

function readlink(path, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  path = getValidatedPath(path, 'oldPath');
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.readlink(pathModule.toNamespacedPath(path), options.encoding, req);
}

function readlinkSync(path, options) {
  options = getOptions(options, {});
  path = getValidatedPath(path, 'oldPath');
  const ctx = { path };
  const result = binding.readlink(pathModule.toNamespacedPath(path),
                                  options.encoding, undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}

function symlink(target, path, type_, callback_) {
  const type = (typeof type_ === 'string' ? type_ : null);
  const callback = makeCallback(arguments[arguments.length - 1]);

  target = getValidatedPath(target, 'target');
  path = getValidatedPath(path);

  const req = new FSReqCallback();
  req.oncomplete = callback;

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
        binding.symlink(preprocessSymlinkDestination(target,
                                                     resolvedType,
                                                     path),
                        pathModule.toNamespacedPath(path), resolvedFlags, req);
      });
      return;
    }
  }

  const flags = stringToSymlinkType(type);
  binding.symlink(preprocessSymlinkDestination(target, type, path),
                  pathModule.toNamespacedPath(path), flags, req);
}

function symlinkSync(target, path, type) {
  type = (typeof type === 'string' ? type : null);
  if (isWindows && type === null) {
    try {
      const absoluteTarget = pathModule.resolve(path, '..', target);
      if (statSync(absoluteTarget).isDirectory()) {
        type = 'dir';
      }
    } catch { }
  }
  target = getValidatedPath(target, 'target');
  path = getValidatedPath(path);
  const flags = stringToSymlinkType(type);

  const ctx = { path: target, dest: path };
  binding.symlink(preprocessSymlinkDestination(target, type, path),
                  pathModule.toNamespacedPath(path), flags, undefined, ctx);

  handleErrorFromBinding(ctx);
}

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

function unlink(path, callback) {
  callback = makeCallback(callback);
  path = getValidatedPath(path);
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.unlink(pathModule.toNamespacedPath(path), req);
}

function unlinkSync(path) {
  path = getValidatedPath(path);
  const ctx = { path };
  binding.unlink(pathModule.toNamespacedPath(path), undefined, ctx);
  handleErrorFromBinding(ctx);
}

function fchmod(fd, mode, callback) {
  validateInt32(fd, 'fd', 0);
  mode = parseFileMode(mode, 'mode');
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.fchmod(fd, mode, req);
}

function fchmodSync(fd, mode) {
  validateInt32(fd, 'fd', 0);
  mode = parseFileMode(mode, 'mode');
  const ctx = {};
  binding.fchmod(fd, mode, undefined, ctx);
  handleErrorFromBinding(ctx);
}

function lchmod(path, mode, callback) {
  callback = maybeCallback(callback);
  fs.open(path, O_WRONLY | O_SYMLINK, (err, fd) => {
    if (err) {
      callback(err);
      return;
    }
    // Prefer to return the chmod error, if one occurs,
    // but still try to close, and report closing errors if they occur.
    fs.fchmod(fd, mode, (err) => {
      fs.close(fd, (err2) => {
        callback(err || err2);
      });
    });
  });
}

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


function chmod(path, mode, callback) {
  path = getValidatedPath(path);
  mode = parseFileMode(mode, 'mode');
  callback = makeCallback(callback);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.chmod(pathModule.toNamespacedPath(path), mode, req);
}

function chmodSync(path, mode) {
  path = getValidatedPath(path);
  mode = parseFileMode(mode, 'mode');

  const ctx = { path };
  binding.chmod(pathModule.toNamespacedPath(path), mode, undefined, ctx);
  handleErrorFromBinding(ctx);
}

function lchown(path, uid, gid, callback) {
  callback = makeCallback(callback);
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.lchown(pathModule.toNamespacedPath(path), uid, gid, req);
}

function lchownSync(path, uid, gid) {
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  const ctx = { path };
  binding.lchown(pathModule.toNamespacedPath(path), uid, gid, undefined, ctx);
  handleErrorFromBinding(ctx);
}

function fchown(fd, uid, gid, callback) {
  validateInt32(fd, 'fd', 0);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);

  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.fchown(fd, uid, gid, req);
}

function fchownSync(fd, uid, gid) {
  validateInt32(fd, 'fd', 0);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);

  const ctx = {};
  binding.fchown(fd, uid, gid, undefined, ctx);
  handleErrorFromBinding(ctx);
}

function chown(path, uid, gid, callback) {
  callback = makeCallback(callback);
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);

  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.chown(pathModule.toNamespacedPath(path), uid, gid, req);
}

function chownSync(path, uid, gid) {
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);
  const ctx = { path };
  binding.chown(pathModule.toNamespacedPath(path), uid, gid, undefined, ctx);
  handleErrorFromBinding(ctx);
}

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

function utimesSync(path, atime, mtime) {
  path = getValidatedPath(path);
  const ctx = { path };
  binding.utimes(pathModule.toNamespacedPath(path),
                 toUnixTimestamp(atime), toUnixTimestamp(mtime),
                 undefined, ctx);
  handleErrorFromBinding(ctx);
}

function futimes(fd, atime, mtime, callback) {
  validateInt32(fd, 'fd', 0);
  atime = toUnixTimestamp(atime, 'atime');
  mtime = toUnixTimestamp(mtime, 'mtime');
  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.futimes(fd, atime, mtime, req);
}

function futimesSync(fd, atime, mtime) {
  validateInt32(fd, 'fd', 0);
  atime = toUnixTimestamp(atime, 'atime');
  mtime = toUnixTimestamp(mtime, 'mtime');
  const ctx = {};
  binding.futimes(fd, atime, mtime, undefined, ctx);
  handleErrorFromBinding(ctx);
}

function writeAll(fd, isUserFd, buffer, offset, length, callback) {
  // write(fd, buffer, offset, length, position, callback)
  fs.write(fd, buffer, offset, length, null, (writeErr, written) => {
    if (writeErr) {
      if (isUserFd) {
        callback(writeErr);
      } else {
        fs.close(fd, function close() {
          callback(writeErr);
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
      writeAll(fd, isUserFd, buffer, offset, length, callback);
    }
  });
}

function writeFile(path, data, options, callback) {
  callback = maybeCallback(callback || options);
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'w' });
  const flag = options.flag || 'w';

  if (!isArrayBufferView(data)) {
    validateStringAfterArrayBufferView(data, 'data');
    data = Buffer.from(data, options.encoding || 'utf8');
  }

  if (isFd(path)) {
    const isUserFd = true;
    writeAll(path, isUserFd, data, 0, data.byteLength, callback);
    return;
  }

  fs.open(path, flag, options.mode, (openErr, fd) => {
    if (openErr) {
      callback(openErr);
    } else {
      const isUserFd = false;
      writeAll(fd, isUserFd, data, 0, data.byteLength, callback);
    }
  });
}

function writeFileSync(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'w' });

  if (!isArrayBufferView(data)) {
    validateStringAfterArrayBufferView(data, 'data');
    data = Buffer.from(data, options.encoding || 'utf8');
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

function appendFileSync(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'a' });

  // Don't make changes directly on options object
  options = copyObject(options);

  // Force append behavior when using a supplied file descriptor
  if (!options.flag || isFd(path))
    options.flag = 'a';

  fs.writeFileSync(path, data, options);
}

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
  if (!watchers)
    watchers = require('internal/fs/watchers');
  const watcher = new watchers.FSWatcher();
  watcher[watchers.kFSWatchStart](filename,
                                  options.persistent,
                                  options.recursive,
                                  options.encoding);

  if (listener) {
    watcher.addListener('change', listener);
  }

  return watcher;
}


const statWatchers = new Map();

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

  if (typeof listener !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('listener', 'Function', listener);
  }

  stat = statWatchers.get(filename);

  if (stat === undefined) {
    if (!watchers)
      watchers = require('internal/fs/watchers');
    stat = new watchers.StatWatcher(options.bigint);
    stat[watchers.kFSStatWatcherStart](filename,
                                       options.persistent, options.interval);
    statWatchers.set(filename, stat);
  }

  stat.addListener('change', listener);
  return stat;
}

function unwatchFile(filename, listener) {
  filename = getValidatedPath(filename);
  filename = pathModule.resolve(filename);
  const stat = statWatchers.get(filename);

  if (stat === undefined) return;

  if (typeof listener === 'function') {
    stat.removeListener('change', listener);
  } else {
    stat.removeAllListeners('change');
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
    return splitRootRe.exec(str)[0];
  };
} else {
  splitRoot = function splitRoot(str) {
    for (let i = 0; i < str.length; ++i) {
      if (str.charCodeAt(i) !== CHAR_FORWARD_SLASH)
        return str.slice(0, i);
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
  } else {
    return asBuffer.toString(options.encoding);
  }
}

// Finds the next portion of a (partial) path, up to the next path delimiter
let nextPart;
if (isWindows) {
  nextPart = function nextPart(p, i) {
    for (; i < p.length; ++i) {
      const ch = p.charCodeAt(i);

      // Check for a separator character
      if (ch === CHAR_BACKWARD_SLASH || ch === CHAR_FORWARD_SLASH)
        return i;
    }
    return -1;
  };
} else {
  nextPart = function nextPart(p, i) { return p.indexOf('/', i); };
}

const emptyObj = ObjectCreate(null);
function realpathSync(p, options) {
  if (!options)
    options = emptyObj;
  else
    options = getOptions(options, emptyObj);
  p = toPathIfFileURL(p);
  if (typeof p !== 'string') {
    p += '';
  }
  validatePath(p);
  p = pathModule.resolve(p);

  const cache = options[realpathCacheKey];
  const maybeCachedResult = cache && cache.get(p);
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
  if (isWindows && !knownHard[base]) {
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
      const last = p.slice(pos);
      current += last;
      base = previous + last;
      pos = p.length;
    } else {
      current += p.slice(pos, result + 1);
      base = previous + p.slice(pos, result);
      pos = result + 1;
    }

    // Continue if not a symlink, break if a pipe/socket
    if (knownHard[base] || (cache && cache.get(base) === base)) {
      if (isFileType(statValues, S_IFIFO) ||
          isFileType(statValues, S_IFSOCK)) {
        break;
      }
      continue;
    }

    let resolvedLink;
    const maybeCachedResolved = cache && cache.get(base);
    if (maybeCachedResolved) {
      resolvedLink = maybeCachedResolved;
    } else {
      // Use stats array directly to avoid creating an fs.Stats instance just
      // for our internal use.

      const baseLong = pathModule.toNamespacedPath(base);
      const ctx = { path: base };
      const stats = binding.lstat(baseLong, false, undefined, ctx);
      handleErrorFromBinding(ctx);

      if (!isFileType(stats, S_IFLNK)) {
        knownHard[base] = true;
        if (cache) cache.set(base, base);
        continue;
      }

      // Read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      let linkTarget = null;
      let id;
      if (!isWindows) {
        const dev = stats[0].toString(32);
        const ino = stats[7].toString(32);
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
    p = pathModule.resolve(resolvedLink, p.slice(pos));

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

  if (cache) cache.set(original, p);
  return encodeRealpathResult(p, options);
}


realpathSync.native = (path, options) => {
  options = getOptions(options, {});
  path = getValidatedPath(path);
  const ctx = { path };
  const result = binding.realpath(path, options.encoding, undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
};


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
      const last = p.slice(pos);
      current += last;
      base = previous + last;
      pos = p.length;
    } else {
      current += p.slice(pos, result + 1);
      base = previous + p.slice(pos, result);
      pos = result + 1;
    }

    // Continue if not a symlink, break if a pipe/socket
    if (knownHard[base]) {
      if (isFileType(statValues, S_IFIFO) ||
          isFileType(statValues, S_IFSOCK)) {
        return callback(null, encodeRealpathResult(p, options));
      }
      return process.nextTick(LOOP);
    }

    return fs.lstat(base, gotStat);
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
      const dev = stats.dev.toString(32);
      const ino = stats.ino.toString(32);
      id = `${dev}:${ino}`;
      if (seenLinks[id]) {
        return gotTarget(null, seenLinks[id], base);
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

  function gotTarget(err, target, base) {
    if (err) return callback(err);

    gotResolvedLink(pathModule.resolve(previous, target));
  }

  function gotResolvedLink(resolvedLink) {
    // Resolve the link, then start over
    p = pathModule.resolve(resolvedLink, p.slice(pos));
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


realpath.native = (path, options, callback) => {
  callback = makeCallback(callback || options);
  options = getOptions(options, {});
  path = getValidatedPath(path);
  const req = new FSReqCallback();
  req.oncomplete = callback;
  return binding.realpath(path, options.encoding, req);
};

function mkdtemp(prefix, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  if (!prefix || typeof prefix !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('prefix', 'string', prefix);
  }
  nullCheck(prefix, 'prefix');
  warnOnNonPortableTemplate(prefix);
  const req = new FSReqCallback();
  req.oncomplete = callback;
  binding.mkdtemp(`${prefix}XXXXXX`, options.encoding, req);
}


function mkdtempSync(prefix, options) {
  options = getOptions(options, {});
  if (!prefix || typeof prefix !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('prefix', 'string', prefix);
  }
  nullCheck(prefix, 'prefix');
  warnOnNonPortableTemplate(prefix);
  const path = `${prefix}XXXXXX`;
  const ctx = { path };
  const result = binding.mkdtemp(path, options.encoding,
                                 undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
}


function copyFile(src, dest, mode, callback) {
  if (typeof mode === 'function') {
    callback = mode;
    mode = 0;
  } else if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK(callback);
  }

  src = getValidatedPath(src, 'src');
  dest = getValidatedPath(dest, 'dest');

  src = pathModule._makeLong(src);
  dest = pathModule._makeLong(dest);
  mode = getValidMode(mode, 'copyFile');
  const req = new FSReqCallback();
  req.oncomplete = makeCallback(callback);
  binding.copyFile(src, dest, mode, req);
}


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
    [ FileReadStream, FileWriteStream ] = [ ReadStream, WriteStream ];
  }
}

function createReadStream(path, options) {
  lazyLoadStreams();
  return new ReadStream(path, options);
}

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
