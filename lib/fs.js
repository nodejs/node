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

const constants = process.binding('constants').fs;
const { S_IFIFO, S_IFLNK, S_IFMT, S_IFREG, S_IFSOCK } = constants;
const util = require('util');
const pathModule = require('path');
const { isUint8Array } = require('internal/util/types');
const { createPromise, promiseResolve } = process.binding('util');

const binding = process.binding('fs');
const fs = exports;
const { Buffer } = require('buffer');
const errors = require('internal/errors');
const {
  ERR_FS_FILE_TOO_LARGE,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_OUT_OF_RANGE
} = errors.codes;
const { Readable, Writable } = require('stream');
const EventEmitter = require('events');
const { FSReqWrap, statValues } = binding;
const { FSEvent } = process.binding('fs_event_wrap');
const internalFS = require('internal/fs');
const { getPathFromURL } = require('internal/url');
const internalUtil = require('internal/util');
const {
  copyObject,
  getOptions,
  isUint32,
  modeNum,
  nullCheck,
  preprocessSymlinkDestination,
  Stats,
  statsFromValues,
  stringToFlags,
  stringToSymlinkType,
  toUnixTimestamp,
  validateBuffer,
  validateLen,
  validateOffsetLengthRead,
  validateOffsetLengthWrite,
  validatePath,
  validateUint32
} = internalFS;
const {
  CHAR_FORWARD_SLASH,
  CHAR_BACKWARD_SLASH,
} = require('internal/constants');

Object.defineProperty(exports, 'constants', {
  configurable: false,
  enumerable: true,
  value: constants
});

let assert_ = null;
function lazyAssert() {
  if (assert_ === null) {
    assert_ = require('assert');
  }
  return assert_;
}

const kMinPoolSpace = 128;
const { kMaxLength } = require('buffer');

const isWindows = process.platform === 'win32';

let truncateWarn = true;

function showTruncateDeprecation() {
  if (truncateWarn) {
    process.emitWarning(
      'Using fs.truncate with a file descriptor is deprecated. Please use ' +
      'fs.ftruncate with a file descriptor instead.',
      'DeprecationWarning', 'DEP0081');
    truncateWarn = false;
  }
}

function handleErrorFromBinding(ctx) {
  if (ctx.errno !== undefined) {  // libuv error numbers
    const err = errors.uvException(ctx);
    Error.captureStackTrace(err, handleErrorFromBinding);
    throw err;
  } else if (ctx.error !== undefined) {  // errors created in C++ land.
    // TODO(joyeecheung): currently, ctx.error are encoding errors
    // usually caused by memory problems. We need to figure out proper error
    // code(s) for this.
    Error.captureStackTrace(ctx.error, handleErrorFromBinding);
    throw ctx.error;
  }
}

function maybeCallback(cb) {
  if (typeof cb === 'function')
    return cb;

  throw new ERR_INVALID_CALLBACK();
}

// Ensure that callbacks run in the global context. Only use this function
// for callbacks that are passed to the binding layer, callbacks that are
// invoked from JS already run in the proper scope.
function makeCallback(cb) {
  if (typeof cb !== 'function') {
    throw new ERR_INVALID_CALLBACK();
  }

  return function(...args) {
    return Reflect.apply(cb, undefined, args);
  };
}

// Special case of `makeCallback()` that is specific to async `*stat()` calls as
// an optimization, since the data passed back to the callback needs to be
// transformed anyway.
function makeStatsCallback(cb) {
  if (typeof cb !== 'function') {
    throw new ERR_INVALID_CALLBACK();
  }

  return function(err) {
    if (err) return cb(err);
    cb(err, statsFromValues());
  };
}

function isFd(path) {
  return (path >>> 0) === path;
}

fs.Stats = Stats;

function isFileType(fileType) {
  // Use stats array directly to avoid creating an fs.Stats instance just for
  // our internal use.
  return (statValues[1/* mode */] & S_IFMT) === fileType;
}

// Don't allow mode to accidentally be overwritten.
Object.defineProperties(fs, {
  F_OK: { enumerable: true, value: constants.F_OK || 0 },
  R_OK: { enumerable: true, value: constants.R_OK || 0 },
  W_OK: { enumerable: true, value: constants.W_OK || 0 },
  X_OK: { enumerable: true, value: constants.X_OK || 0 },
});

fs.access = function(path, mode, callback) {
  if (typeof mode === 'function') {
    callback = mode;
    mode = fs.F_OK;
  }

  path = getPathFromURL(path);
  validatePath(path);

  mode = mode | 0;
  var req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.access(pathModule.toNamespacedPath(path), mode, req);
};

fs.accessSync = function(path, mode) {
  path = getPathFromURL(path);
  validatePath(path);

  if (mode === undefined)
    mode = fs.F_OK;
  else
    mode = mode | 0;

  const ctx = { path };
  binding.access(pathModule.toNamespacedPath(path), mode, undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.exists = function(path, callback) {
  maybeCallback(callback);

  function suppressedCallback(err) {
    callback(err ? false : true);
  }

  try {
    fs.access(path, fs.FS_OK, suppressedCallback);
  } catch (err) {
    return callback(false);
  }
};

Object.defineProperty(fs.exists, internalUtil.promisify.custom, {
  value: (path) => {
    const promise = createPromise();
    fs.exists(path, (exists) => promiseResolve(promise, exists));
    return promise;
  }
});

// fs.existsSync never throws, it only returns true or false.
// Since fs.existsSync never throws, users have established
// the expectation that passing invalid arguments to it, even like
// fs.existsSync(), would only get a false in return, so we cannot signal
// validation errors to users properly out of compatibility concerns.
// TODO(joyeecheung): deprecate the never-throw-on-invalid-arguments behavior
fs.existsSync = function(path) {
  try {
    fs.accessSync(path, fs.FS_OK);
    return true;
  } catch (e) {
    return false;
  }
};

fs.readFile = function(path, options, callback) {
  callback = maybeCallback(callback || options);
  options = getOptions(options, { flag: 'r' });
  var context = new ReadFileContext(callback, options.encoding);
  context.isUserFd = isFd(path); // file descriptor ownership
  var req = new FSReqWrap();
  req.context = context;
  req.oncomplete = readFileAfterOpen;

  if (context.isUserFd) {
    process.nextTick(function() {
      req.oncomplete(null, path);
    });
    return;
  }

  path = getPathFromURL(path);
  validatePath(path);
  binding.open(pathModule.toNamespacedPath(path),
               stringToFlags(options.flag || 'r'),
               0o666,
               req);
};

const kReadFileBufferLength = 8 * 1024;

function ReadFileContext(callback, encoding) {
  this.fd = undefined;
  this.isUserFd = undefined;
  this.size = undefined;
  this.callback = callback;
  this.buffers = null;
  this.buffer = null;
  this.pos = 0;
  this.encoding = encoding;
  this.err = null;
}

ReadFileContext.prototype.read = function() {
  var buffer;
  var offset;
  var length;

  if (this.size === 0) {
    buffer = this.buffer = Buffer.allocUnsafeSlow(kReadFileBufferLength);
    offset = 0;
    length = kReadFileBufferLength;
  } else {
    buffer = this.buffer;
    offset = this.pos;
    length = Math.min(kReadFileBufferLength, this.size - this.pos);
  }

  var req = new FSReqWrap();
  req.oncomplete = readFileAfterRead;
  req.context = this;

  binding.read(this.fd, buffer, offset, length, -1, req);
};

ReadFileContext.prototype.close = function(err) {
  var req = new FSReqWrap();
  req.oncomplete = readFileAfterClose;
  req.context = this;
  this.err = err;

  if (this.isUserFd) {
    process.nextTick(function() {
      req.oncomplete(null);
    });
    return;
  }

  binding.close(this.fd, req);
};

function readFileAfterOpen(err, fd) {
  var context = this.context;

  if (err) {
    context.callback(err);
    return;
  }

  context.fd = fd;

  var req = new FSReqWrap();
  req.oncomplete = readFileAfterStat;
  req.context = context;
  binding.fstat(fd, req);
}

function readFileAfterStat(err) {
  var context = this.context;

  if (err)
    return context.close(err);

  var size;
  if (isFileType(S_IFREG))
    size = context.size = statValues[8];
  else
    size = context.size = 0;

  if (size === 0) {
    context.buffers = [];
    context.read();
    return;
  }

  if (size > kMaxLength) {
    err = new ERR_FS_FILE_TOO_LARGE(size);
    return context.close(err);
  }

  try {
    context.buffer = Buffer.allocUnsafeSlow(size);
  } catch (err) {
    return context.close(err);
  }
  context.read();
}

function readFileAfterRead(err, bytesRead) {
  var context = this.context;

  if (err)
    return context.close(err);

  if (bytesRead === 0)
    return context.close();

  context.pos += bytesRead;

  if (context.size !== 0) {
    if (context.pos === context.size)
      context.close();
    else
      context.read();
  } else {
    // unknown size, just read until we don't get bytes.
    context.buffers.push(context.buffer.slice(0, bytesRead));
    context.read();
  }
}

function readFileAfterClose(err) {
  var context = this.context;
  var buffer = null;
  var callback = context.callback;

  if (context.err || err)
    return callback(context.err || err);

  try {
    if (context.size === 0)
      buffer = Buffer.concat(context.buffers, context.pos);
    else if (context.pos < context.size)
      buffer = context.buffer.slice(0, context.pos);
    else
      buffer = context.buffer;

    if (context.encoding)
      buffer = buffer.toString(context.encoding);
  } catch (err) {
    return callback(err);
  }

  callback(null, buffer);
}

function tryStatSync(fd, isUserFd) {
  const ctx = {};
  binding.fstat(fd, undefined, ctx);
  if (ctx.errno !== undefined && !isUserFd) {
    fs.closeSync(fd);
    throw errors.uvException(ctx);
  }
}

function tryCreateBuffer(size, fd, isUserFd) {
  var threw = true;
  var buffer;
  try {
    if (size > kMaxLength) {
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
  var threw = true;
  var bytesRead;
  try {
    bytesRead = fs.readSync(fd, buffer, pos, len);
    threw = false;
  } finally {
    if (threw && !isUserFd) fs.closeSync(fd);
  }
  return bytesRead;
}

fs.readFileSync = function(path, options) {
  options = getOptions(options, { flag: 'r' });
  var isUserFd = isFd(path); // file descriptor ownership
  var fd = isUserFd ? path : fs.openSync(path, options.flag || 'r', 0o666);

  tryStatSync(fd, isUserFd);
  var size;
  if (isFileType(S_IFREG))
    size = statValues[8];
  else
    size = 0;
  var pos = 0;
  var buffer; // single buffer with file data
  var buffers; // list for when size is unknown

  if (size === 0) {
    buffers = [];
  } else {
    buffer = tryCreateBuffer(size, fd, isUserFd);
  }

  var bytesRead;

  if (size !== 0) {
    do {
      bytesRead = tryReadSync(fd, isUserFd, buffer, pos, size - pos);
      pos += bytesRead;
    } while (bytesRead !== 0 && pos < size);
  } else {
    do {
      // the kernel lies about many files.
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
    // data was collected into the buffers list.
    buffer = Buffer.concat(buffers, pos);
  } else if (pos < size) {
    buffer = buffer.slice(0, pos);
  }

  if (options.encoding) buffer = buffer.toString(options.encoding);
  return buffer;
};

fs.close = function(fd, callback) {
  validateUint32(fd, 'fd');
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.close(fd, req);
};

fs.closeSync = function(fd) {
  validateUint32(fd, 'fd');

  const ctx = {};
  binding.close(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.open = function(path, flags, mode, callback_) {
  var callback = makeCallback(arguments[arguments.length - 1]);
  mode = modeNum(mode, 0o666);

  path = getPathFromURL(path);
  validatePath(path);
  validateUint32(mode, 'mode');

  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.open(pathModule.toNamespacedPath(path),
               stringToFlags(flags),
               mode,
               req);
};

fs.openSync = function(path, flags, mode) {
  mode = modeNum(mode, 0o666);
  path = getPathFromURL(path);
  validatePath(path);
  validateUint32(mode, 'mode');

  const ctx = { path };
  const result = binding.open(pathModule.toNamespacedPath(path),
                              stringToFlags(flags), mode,
                              undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
};

fs.read = function(fd, buffer, offset, length, position, callback) {
  validateUint32(fd, 'fd');
  validateBuffer(buffer);

  offset |= 0;
  length |= 0;

  if (length === 0) {
    return process.nextTick(function() {
      callback && callback(null, 0, buffer);
    });
  }

  validateOffsetLengthRead(offset, length, buffer.length);

  if (!isUint32(position))
    position = -1;

  function wrapper(err, bytesRead) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback && callback(err, bytesRead || 0, buffer);
  }

  const req = new FSReqWrap();
  req.oncomplete = wrapper;

  binding.read(fd, buffer, offset, length, position, req);
};

Object.defineProperty(fs.read, internalUtil.customPromisifyArgs,
                      { value: ['bytesRead', 'buffer'], enumerable: false });

fs.readSync = function(fd, buffer, offset, length, position) {
  validateUint32(fd, 'fd');
  validateBuffer(buffer);

  offset |= 0;
  length |= 0;

  if (length === 0) {
    return 0;
  }

  validateOffsetLengthRead(offset, length, buffer.length);

  if (!isUint32(position))
    position = -1;

  const ctx = {};
  const result = binding.read(fd, buffer, offset, length, position,
                              undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
};

// usage:
//  fs.write(fd, buffer[, offset[, length[, position]]], callback);
// OR
//  fs.write(fd, string[, position[, encoding]], callback);
fs.write = function(fd, buffer, offset, length, position, callback) {
  function wrapper(err, written) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback(err, written || 0, buffer);
  }

  validateUint32(fd, 'fd');

  const req = new FSReqWrap();
  req.oncomplete = wrapper;

  if (isUint8Array(buffer)) {
    callback = maybeCallback(callback || position || length || offset);
    if (typeof offset !== 'number')
      offset = 0;
    if (typeof length !== 'number')
      length = buffer.length - offset;
    if (typeof position !== 'number')
      position = null;
    validateOffsetLengthWrite(offset, length, buffer.byteLength);
    return binding.writeBuffer(fd, buffer, offset, length, position, req);
  }

  if (typeof buffer !== 'string')
    buffer += '';
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
  return binding.writeString(fd, buffer, offset, length, req);
};

Object.defineProperty(fs.write, internalUtil.customPromisifyArgs,
                      { value: ['bytesWritten', 'buffer'], enumerable: false });

// usage:
//  fs.writeSync(fd, buffer[, offset[, length[, position]]]);
// OR
//  fs.writeSync(fd, string[, position[, encoding]]);
fs.writeSync = function(fd, buffer, offset, length, position) {
  validateUint32(fd, 'fd');
  const ctx = {};
  let result;
  if (isUint8Array(buffer)) {
    if (position === undefined)
      position = null;
    if (typeof offset !== 'number')
      offset = 0;
    if (typeof length !== 'number')
      length = buffer.length - offset;
    validateOffsetLengthWrite(offset, length, buffer.byteLength);
    result = binding.writeBuffer(fd, buffer, offset, length, position,
                                 undefined, ctx);
  } else {
    if (typeof buffer !== 'string')
      buffer += '';
    if (offset === undefined)
      offset = null;
    result = binding.writeString(fd, buffer, offset, length,
                                 undefined, ctx);
  }
  handleErrorFromBinding(ctx);
  return result;
};

fs.rename = function(oldPath, newPath, callback) {
  callback = makeCallback(callback);
  oldPath = getPathFromURL(oldPath);
  validatePath(oldPath, 'oldPath');
  newPath = getPathFromURL(newPath);
  validatePath(newPath, 'newPath');
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.rename(pathModule.toNamespacedPath(oldPath),
                 pathModule.toNamespacedPath(newPath),
                 req);
};

fs.renameSync = function(oldPath, newPath) {
  oldPath = getPathFromURL(oldPath);
  validatePath(oldPath, 'oldPath');
  newPath = getPathFromURL(newPath);
  validatePath(newPath, 'newPath');
  const ctx = { path: oldPath, dest: newPath };
  binding.rename(pathModule.toNamespacedPath(oldPath),
                 pathModule.toNamespacedPath(newPath), undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.truncate = function(path, len, callback) {
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

  callback = maybeCallback(callback);
  fs.open(path, 'r+', function(er, fd) {
    if (er) return callback(er);
    var req = new FSReqWrap();
    req.oncomplete = function oncomplete(er) {
      fs.close(fd, function(er2) {
        callback(er || er2);
      });
    };
    binding.ftruncate(fd, len, req);
  });
};

fs.truncateSync = function(path, len) {
  if (typeof path === 'number') {
    // legacy
    showTruncateDeprecation();
    return fs.ftruncateSync(path, len);
  }
  if (len === undefined) {
    len = 0;
  }
  // allow error to be thrown, but still close fd.
  var fd = fs.openSync(path, 'r+');
  var ret;

  try {
    ret = fs.ftruncateSync(fd, len);
  } finally {
    fs.closeSync(fd);
  }
  return ret;
};

fs.ftruncate = function(fd, len = 0, callback) {
  if (typeof len === 'function') {
    callback = len;
    len = 0;
  }
  validateUint32(fd, 'fd');
  // TODO(BridgeAR): This does not seem right.
  // There does not seem to be any validation before and if there is any, it
  // should work similar to validateUint32 or not have a upper cap at all.
  // This applies to all usage of `validateLen`.
  validateLen(len);
  len = Math.max(0, len);
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.ftruncate(fd, len, req);
};

fs.ftruncateSync = function(fd, len = 0) {
  validateUint32(fd, 'fd');
  validateLen(len);
  len = Math.max(0, len);
  const ctx = {};
  binding.ftruncate(fd, len, undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.rmdir = function(path, callback) {
  callback = makeCallback(callback);
  path = getPathFromURL(path);
  validatePath(path);
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.rmdir(pathModule.toNamespacedPath(path), req);
};

fs.rmdirSync = function(path) {
  path = getPathFromURL(path);
  validatePath(path);
  const ctx = { path };
  binding.rmdir(pathModule.toNamespacedPath(path), undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.fdatasync = function(fd, callback) {
  validateUint32(fd, 'fd');
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fdatasync(fd, req);
};

fs.fdatasyncSync = function(fd) {
  validateUint32(fd, 'fd');
  const ctx = {};
  binding.fdatasync(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.fsync = function(fd, callback) {
  validateUint32(fd, 'fd');
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fsync(fd, req);
};

fs.fsyncSync = function(fd) {
  validateUint32(fd, 'fd');
  const ctx = {};
  binding.fsync(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.mkdir = function(path, mode, callback) {
  if (typeof mode === 'function') callback = mode;
  callback = makeCallback(callback);
  path = getPathFromURL(path);
  validatePath(path);
  mode = modeNum(mode, 0o777);
  validateUint32(mode, 'mode');

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.mkdir(pathModule.toNamespacedPath(path), mode, req);
};

fs.mkdirSync = function(path, mode) {
  path = getPathFromURL(path);
  validatePath(path);
  mode = modeNum(mode, 0o777);
  validateUint32(mode, 'mode');
  const ctx = { path };
  binding.mkdir(pathModule.toNamespacedPath(path), mode, undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.readdir = function(path, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  path = getPathFromURL(path);
  validatePath(path);

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readdir(pathModule.toNamespacedPath(path), options.encoding, req);
};

fs.readdirSync = function(path, options) {
  options = getOptions(options, {});
  path = getPathFromURL(path);
  validatePath(path);
  const ctx = { path };
  const result = binding.readdir(pathModule.toNamespacedPath(path),
                                 options.encoding, undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
};

fs.fstat = function(fd, callback) {
  validateUint32(fd, 'fd');
  const req = new FSReqWrap();
  req.oncomplete = makeStatsCallback(callback);
  binding.fstat(fd, req);
};

fs.lstat = function(path, callback) {
  callback = makeStatsCallback(callback);
  path = getPathFromURL(path);
  validatePath(path);
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.lstat(pathModule.toNamespacedPath(path), req);
};

fs.stat = function(path, callback) {
  callback = makeStatsCallback(callback);
  path = getPathFromURL(path);
  validatePath(path);
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.stat(pathModule.toNamespacedPath(path), req);
};

fs.fstatSync = function(fd) {
  validateUint32(fd, 'fd');
  const ctx = { fd };
  binding.fstat(fd, undefined, ctx);
  handleErrorFromBinding(ctx);
  return statsFromValues();
};

fs.lstatSync = function(path) {
  path = getPathFromURL(path);
  validatePath(path);
  const ctx = { path };
  binding.lstat(pathModule.toNamespacedPath(path), undefined, ctx);
  handleErrorFromBinding(ctx);
  return statsFromValues();
};

fs.statSync = function(path) {
  path = getPathFromURL(path);
  validatePath(path);
  const ctx = { path };
  binding.stat(pathModule.toNamespacedPath(path), undefined, ctx);
  handleErrorFromBinding(ctx);
  return statsFromValues();
};

fs.readlink = function(path, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  path = getPathFromURL(path);
  validatePath(path, 'oldPath');
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readlink(pathModule.toNamespacedPath(path), options.encoding, req);
};

fs.readlinkSync = function(path, options) {
  options = getOptions(options, {});
  path = getPathFromURL(path);
  validatePath(path, 'oldPath');
  const ctx = { path };
  const result = binding.readlink(pathModule.toNamespacedPath(path),
                                  options.encoding, undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
};

fs.symlink = function(target, path, type_, callback_) {
  var type = (typeof type_ === 'string' ? type_ : null);
  var callback = makeCallback(arguments[arguments.length - 1]);

  target = getPathFromURL(target);
  path = getPathFromURL(path);
  validatePath(target, 'target');
  validatePath(path);

  const flags = stringToSymlinkType(type);
  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.symlink(preprocessSymlinkDestination(target, type, path),
                  pathModule.toNamespacedPath(path), flags, req);
};

fs.symlinkSync = function(target, path, type) {
  type = (typeof type === 'string' ? type : null);
  target = getPathFromURL(target);
  path = getPathFromURL(path);
  validatePath(target, 'target');
  validatePath(path);
  const flags = stringToSymlinkType(type);

  const ctx = { path: target, dest: path };
  binding.symlink(preprocessSymlinkDestination(target, type, path),
                  pathModule.toNamespacedPath(path), flags, undefined, ctx);

  handleErrorFromBinding(ctx);
};

fs.link = function(existingPath, newPath, callback) {
  callback = makeCallback(callback);

  existingPath = getPathFromURL(existingPath);
  newPath = getPathFromURL(newPath);
  validatePath(existingPath, 'existingPath');
  validatePath(newPath, 'newPath');

  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.link(pathModule.toNamespacedPath(existingPath),
               pathModule.toNamespacedPath(newPath),
               req);
};

fs.linkSync = function(existingPath, newPath) {
  existingPath = getPathFromURL(existingPath);
  newPath = getPathFromURL(newPath);
  validatePath(existingPath, 'existingPath');
  validatePath(newPath, 'newPath');

  const ctx = { path: existingPath, dest: newPath };
  const result = binding.link(pathModule.toNamespacedPath(existingPath),
                              pathModule.toNamespacedPath(newPath),
                              undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
};

fs.unlink = function(path, callback) {
  callback = makeCallback(callback);
  path = getPathFromURL(path);
  validatePath(path);
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.unlink(pathModule.toNamespacedPath(path), req);
};

fs.unlinkSync = function(path) {
  path = getPathFromURL(path);
  validatePath(path);
  const ctx = { path };
  binding.unlink(pathModule.toNamespacedPath(path), undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.fchmod = function(fd, mode, callback) {
  mode = modeNum(mode);
  validateUint32(fd, 'fd');
  validateUint32(mode, 'mode');
  // Values for mode < 0 are already checked via the validateUint32 function
  if (mode > 0o777)
    throw new ERR_OUT_OF_RANGE('mode');

  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fchmod(fd, mode, req);
};

fs.fchmodSync = function(fd, mode) {
  mode = modeNum(mode);
  validateUint32(fd, 'fd');
  validateUint32(mode, 'mode');
  // Values for mode < 0 are already checked via the validateUint32 function
  if (mode > 0o777)
    throw new ERR_OUT_OF_RANGE('mode');
  const ctx = {};
  binding.fchmod(fd, mode, undefined, ctx);
  handleErrorFromBinding(ctx);
};

if (constants.O_SYMLINK !== undefined) {
  fs.lchmod = function(path, mode, callback) {
    callback = maybeCallback(callback);
    fs.open(path, constants.O_WRONLY | constants.O_SYMLINK, function(err, fd) {
      if (err) {
        callback(err);
        return;
      }
      // Prefer to return the chmod error, if one occurs,
      // but still try to close, and report closing errors if they occur.
      fs.fchmod(fd, mode, function(err) {
        fs.close(fd, function(err2) {
          callback(err || err2);
        });
      });
    });
  };

  fs.lchmodSync = function(path, mode) {
    const fd = fs.openSync(path, constants.O_WRONLY | constants.O_SYMLINK);

    // Prefer to return the chmod error, if one occurs,
    // but still try to close, and report closing errors if they occur.
    let ret;
    try {
      ret = fs.fchmodSync(fd, mode);
    } finally {
      fs.closeSync(fd);
    }
    return ret;
  };
}


fs.chmod = function(path, mode, callback) {
  callback = makeCallback(callback);
  path = getPathFromURL(path);
  validatePath(path);
  mode = modeNum(mode);
  validateUint32(mode, 'mode');

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.chmod(pathModule.toNamespacedPath(path), mode, req);
};

fs.chmodSync = function(path, mode) {
  path = getPathFromURL(path);
  validatePath(path);
  mode = modeNum(mode);
  validateUint32(mode, 'mode');
  const ctx = { path };
  binding.chmod(pathModule.toNamespacedPath(path), mode, undefined, ctx);
  handleErrorFromBinding(ctx);
};

if (constants.O_SYMLINK !== undefined) {
  fs.lchown = function(path, uid, gid, callback) {
    callback = maybeCallback(callback);
    fs.open(path, constants.O_WRONLY | constants.O_SYMLINK, function(err, fd) {
      if (err) {
        callback(err);
        return;
      }
      // Prefer to return the chown error, if one occurs,
      // but still try to close, and report closing errors if they occur.
      fs.fchown(fd, uid, gid, function(err) {
        fs.close(fd, function(err2) {
          callback(err || err2);
        });
      });
    });
  };

  fs.lchownSync = function(path, uid, gid) {
    const fd = fs.openSync(path, constants.O_WRONLY | constants.O_SYMLINK);
    let ret;
    try {
      ret = fs.fchownSync(fd, uid, gid);
    } finally {
      fs.closeSync(fd);
    }
    return ret;
  };
}

fs.fchown = function(fd, uid, gid, callback) {
  validateUint32(fd, 'fd');
  validateUint32(uid, 'uid');
  validateUint32(gid, 'gid');

  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fchown(fd, uid, gid, req);
};

fs.fchownSync = function(fd, uid, gid) {
  validateUint32(fd, 'fd');
  validateUint32(uid, 'uid');
  validateUint32(gid, 'gid');

  const ctx = {};
  binding.fchown(fd, uid, gid, undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.chown = function(path, uid, gid, callback) {
  callback = makeCallback(callback);
  path = getPathFromURL(path);
  validatePath(path);
  validateUint32(uid, 'uid');
  validateUint32(gid, 'gid');

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.chown(pathModule.toNamespacedPath(path), uid, gid, req);
};

fs.chownSync = function(path, uid, gid) {
  path = getPathFromURL(path);
  validatePath(path);
  validateUint32(uid, 'uid');
  validateUint32(gid, 'gid');
  const ctx = { path };
  binding.chown(pathModule.toNamespacedPath(path), uid, gid, undefined, ctx);
  handleErrorFromBinding(ctx);
};

// exported for unit tests, not for public consumption
fs._toUnixTimestamp = toUnixTimestamp;

fs.utimes = function(path, atime, mtime, callback) {
  callback = makeCallback(callback);
  path = getPathFromURL(path);
  validatePath(path);

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.utimes(pathModule.toNamespacedPath(path),
                 toUnixTimestamp(atime),
                 toUnixTimestamp(mtime),
                 req);
};

fs.utimesSync = function(path, atime, mtime) {
  path = getPathFromURL(path);
  validatePath(path);
  const ctx = { path };
  binding.utimes(pathModule.toNamespacedPath(path),
                 toUnixTimestamp(atime), toUnixTimestamp(mtime),
                 undefined, ctx);
  handleErrorFromBinding(ctx);
};

fs.futimes = function(fd, atime, mtime, callback) {
  validateUint32(fd, 'fd');
  atime = toUnixTimestamp(atime, 'atime');
  mtime = toUnixTimestamp(mtime, 'mtime');
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.futimes(fd, atime, mtime, req);
};

fs.futimesSync = function(fd, atime, mtime) {
  validateUint32(fd, 'fd');
  atime = toUnixTimestamp(atime, 'atime');
  mtime = toUnixTimestamp(mtime, 'mtime');
  const ctx = {};
  binding.futimes(fd, atime, mtime, undefined, ctx);
  handleErrorFromBinding(ctx);
};

function writeAll(fd, isUserFd, buffer, offset, length, position, callback) {
  // write(fd, buffer, offset, length, position, callback)
  fs.write(fd, buffer, offset, length, position, function(writeErr, written) {
    if (writeErr) {
      if (isUserFd) {
        callback(writeErr);
      } else {
        fs.close(fd, function() {
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
      if (position !== null) {
        position += written;
      }
      writeAll(fd, isUserFd, buffer, offset, length, position, callback);
    }
  });
}

fs.writeFile = function(path, data, options, callback) {
  callback = maybeCallback(callback || options);
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'w' });
  const flag = options.flag || 'w';

  if (isFd(path)) {
    writeFd(path, true);
    return;
  }

  fs.open(path, flag, options.mode, function(openErr, fd) {
    if (openErr) {
      callback(openErr);
    } else {
      writeFd(fd, false);
    }
  });

  function writeFd(fd, isUserFd) {
    var buffer = isUint8Array(data) ?
      data : Buffer.from('' + data, options.encoding || 'utf8');
    var position = /a/.test(flag) ? null : 0;

    writeAll(fd, isUserFd, buffer, 0, buffer.length, position, callback);
  }
};

fs.writeFileSync = function(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'w' });
  const flag = options.flag || 'w';

  var isUserFd = isFd(path); // file descriptor ownership
  var fd = isUserFd ? path : fs.openSync(path, flag, options.mode);

  if (!isUint8Array(data)) {
    data = Buffer.from('' + data, options.encoding || 'utf8');
  }
  var offset = 0;
  var length = data.length;
  var position = /a/.test(flag) ? null : 0;
  try {
    while (length > 0) {
      var written = fs.writeSync(fd, data, offset, length, position);
      offset += written;
      length -= written;
      if (position !== null) {
        position += written;
      }
    }
  } finally {
    if (!isUserFd) fs.closeSync(fd);
  }
};

fs.appendFile = function(path, data, options, callback) {
  callback = maybeCallback(callback || options);
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'a' });

  // Don't make changes directly on options object
  options = copyObject(options);

  // force append behavior when using a supplied file descriptor
  if (!options.flag || isFd(path))
    options.flag = 'a';

  fs.writeFile(path, data, options, callback);
};

fs.appendFileSync = function(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'a' });

  // Don't make changes directly on options object
  options = copyObject(options);

  // force append behavior when using a supplied file descriptor
  if (!options.flag || isFd(path))
    options.flag = 'a';

  fs.writeFileSync(path, data, options);
};

function FSWatcher() {
  EventEmitter.call(this);

  var self = this;
  this._handle = new FSEvent();
  this._handle.owner = this;

  this._handle.onchange = function(status, eventType, filename) {
    // TODO(joyeecheung): we may check self._handle.initialized here
    // and return if that is false. This allows us to avoid firing the event
    // after the handle is closed, and to fire both UV_RENAME and UV_CHANGE
    // if they are set by libuv at the same time.
    if (status < 0) {
      self._handle.close();
      const error = errors.uvException({
        errno: status,
        syscall: 'watch',
        path: filename
      });
      error.filename = filename;
      self.emit('error', error);
    } else {
      self.emit('change', eventType, filename);
    }
  };
}
util.inherits(FSWatcher, EventEmitter);

// FIXME(joyeecheung): this method is not documented.
// At the moment if filename is undefined, we
// 1. Throw an Error if it's the first time .start() is called
// 2. Return silently if .start() has already been called
//    on a valid filename and the wrap has been initialized
// This method is a noop if the watcher has already been started.
FSWatcher.prototype.start = function(filename,
                                     persistent,
                                     recursive,
                                     encoding) {
  lazyAssert()(this._handle instanceof FSEvent, 'handle must be a FSEvent');
  if (this._handle.initialized) {
    return;
  }

  filename = getPathFromURL(filename);
  validatePath(filename, 'filename');

  const err = this._handle.start(pathModule.toNamespacedPath(filename),
                                 persistent,
                                 recursive,
                                 encoding);
  if (err) {
    const error = errors.uvException({
      errno: err,
      syscall: 'watch',
      path: filename
    });
    error.filename = filename;
    throw error;
  }
};

// This method is a noop if the watcher has not been started.
FSWatcher.prototype.close = function() {
  lazyAssert()(this._handle instanceof FSEvent, 'handle must be a FSEvent');
  if (!this._handle.initialized) {
    return;
  }
  this._handle.close();
};

fs.watch = function(filename, options, listener) {
  if (typeof options === 'function') {
    listener = options;
  }
  options = getOptions(options, {});

  // Don't make changes directly on options object
  options = copyObject(options);

  if (options.persistent === undefined) options.persistent = true;
  if (options.recursive === undefined) options.recursive = false;

  const watcher = new FSWatcher();
  watcher.start(filename,
                options.persistent,
                options.recursive,
                options.encoding);

  if (listener) {
    watcher.addListener('change', listener);
  }

  return watcher;
};


// Stat Change Watchers

function emitStop(self) {
  self.emit('stop');
}

function statsFromPrevValues() {
  return new Stats(statValues[14], statValues[15], statValues[16],
                   statValues[17], statValues[18], statValues[19],
                   statValues[20] < 0 ? undefined : statValues[20],
                   statValues[21], statValues[22],
                   statValues[23] < 0 ? undefined : statValues[23],
                   statValues[24], statValues[25], statValues[26],
                   statValues[27]);
}
function StatWatcher() {
  EventEmitter.call(this);

  var self = this;
  this._handle = new binding.StatWatcher();

  // uv_fs_poll is a little more powerful than ev_stat but we curb it for
  // the sake of backwards compatibility
  var oldStatus = -1;

  this._handle.onchange = function(newStatus) {
    if (oldStatus === -1 &&
        newStatus === -1 &&
        statValues[2/* new nlink */] === statValues[16/* old nlink */]) return;

    oldStatus = newStatus;
    self.emit('change', statsFromValues(), statsFromPrevValues());
  };

  this._handle.onstop = function() {
    process.nextTick(emitStop, self);
  };
}
util.inherits(StatWatcher, EventEmitter);


// FIXME(joyeecheung): this method is not documented.
// At the moment if filename is undefined, we
// 1. Throw an Error if it's the first time .start() is called
// 2. Return silently if .start() has already been called
//    on a valid filename and the wrap has been initialized
// This method is a noop if the watcher has already been started.
StatWatcher.prototype.start = function(filename, persistent, interval) {
  lazyAssert()(this._handle instanceof binding.StatWatcher,
               'handle must be a StatWatcher');
  if (this._handle.isActive) {
    return;
  }

  filename = getPathFromURL(filename);
  validatePath(filename, 'filename');
  validateUint32(interval, 'interval');
  const err = this._handle.start(pathModule.toNamespacedPath(filename),
                                 persistent, interval);
  if (err) {
    const error = errors.uvException({
      errno: err,
      syscall: 'watch',
      path: filename
    });
    error.filename = filename;
    throw error;
  }
};

// FIXME(joyeecheung): this method is not documented while there is
// another documented fs.unwatchFile(). The counterpart in
// FSWatcher is .close()
// This method is a noop if the watcher has not been started.
StatWatcher.prototype.stop = function() {
  lazyAssert()(this._handle instanceof binding.StatWatcher,
               'handle must be a StatWatcher');
  if (!this._handle.isActive) {
    return;
  }
  this._handle.stop();
};


const statWatchers = new Map();

fs.watchFile = function(filename, options, listener) {
  filename = getPathFromURL(filename);
  validatePath(filename);
  filename = pathModule.resolve(filename);
  var stat;

  var defaults = {
    // Poll interval in milliseconds. 5007 is what libev used to use. It's
    // a little on the slow side but let's stick with it for now to keep
    // behavioral changes to a minimum.
    interval: 5007,
    persistent: true
  };

  if (options !== null && typeof options === 'object') {
    options = util._extend(defaults, options);
  } else {
    listener = options;
    options = defaults;
  }

  if (typeof listener !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('listener', 'Function', listener);
  }

  stat = statWatchers.get(filename);

  if (stat === undefined) {
    stat = new StatWatcher();
    stat.start(filename, options.persistent, options.interval);
    statWatchers.set(filename, stat);
  }

  stat.addListener('change', listener);
  return stat;
};

fs.unwatchFile = function(filename, listener) {
  filename = getPathFromURL(filename);
  validatePath(filename);
  filename = pathModule.resolve(filename);
  var stat = statWatchers.get(filename);

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
};


var splitRoot;
if (isWindows) {
  // Regex to find the device root on Windows (e.g. 'c:\\'), including trailing
  // slash.
  const splitRootRe = /^(?:[a-zA-Z]:|[\\/]{2}[^\\/]+[\\/][^\\/]+)?[\\/]*/;
  splitRoot = function splitRoot(str) {
    return splitRootRe.exec(str)[0];
  };
} else {
  splitRoot = function splitRoot(str) {
    for (var i = 0; i < str.length; ++i) {
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
var nextPart;
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

const emptyObj = Object.create(null);
fs.realpathSync = function realpathSync(p, options) {
  if (!options)
    options = emptyObj;
  else
    options = getOptions(options, emptyObj);
  p = getPathFromURL(p);
  if (typeof p !== 'string') {
    p += '';
  }
  validatePath(p);
  p = pathModule.resolve(p);

  const cache = options[internalFS.realpathCacheKey];
  const maybeCachedResult = cache && cache.get(p);
  if (maybeCachedResult) {
    return maybeCachedResult;
  }

  const seenLinks = Object.create(null);
  const knownHard = Object.create(null);
  const original = p;

  // current character position in p
  var pos;
  // the partial path so far, including a trailing slash if any
  var current;
  // the partial path without a trailing slash (except when pointing at a root)
  var base;
  // the partial path scanned in the previous round, with slash
  var previous;

  // Skip over roots
  current = base = splitRoot(p);
  pos = current.length;

  // On windows, check that the root exists. On unix there is no need.
  if (isWindows && !knownHard[base]) {
    const ctx = { path: base };
    binding.lstat(pathModule.toNamespacedPath(base), undefined, ctx);
    handleErrorFromBinding(ctx);
    knownHard[base] = true;
  }

  // walk down the path, swapping out linked path parts for their real
  // values
  // NB: p.length changes.
  while (pos < p.length) {
    // find the next part
    var result = nextPart(p, pos);
    previous = current;
    if (result === -1) {
      var last = p.slice(pos);
      current += last;
      base = previous + last;
      pos = p.length;
    } else {
      current += p.slice(pos, result + 1);
      base = previous + p.slice(pos, result);
      pos = result + 1;
    }

    // continue if not a symlink, break if a pipe/socket
    if (knownHard[base] || (cache && cache.get(base) === base)) {
      if (isFileType(S_IFIFO) || isFileType(S_IFSOCK)) {
        break;
      }
      continue;
    }

    var resolvedLink;
    var maybeCachedResolved = cache && cache.get(base);
    if (maybeCachedResolved) {
      resolvedLink = maybeCachedResolved;
    } else {
      // Use stats array directly to avoid creating an fs.Stats instance just
      // for our internal use.

      var baseLong = pathModule.toNamespacedPath(base);
      const ctx = { path: base };
      binding.lstat(baseLong, undefined, ctx);
      handleErrorFromBinding(ctx);

      if (!isFileType(S_IFLNK)) {
        knownHard[base] = true;
        if (cache) cache.set(base, base);
        continue;
      }

      // read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      var linkTarget = null;
      var id;
      if (!isWindows) {
        var dev = statValues[0].toString(32);
        var ino = statValues[7].toString(32);
        id = `${dev}:${ino}`;
        if (seenLinks[id]) {
          linkTarget = seenLinks[id];
        }
      }
      if (linkTarget === null) {
        const ctx = { path: base };
        binding.stat(baseLong, undefined, ctx);
        handleErrorFromBinding(ctx);
        linkTarget = binding.readlink(baseLong, undefined, undefined, ctx);
        handleErrorFromBinding(ctx);
      }
      resolvedLink = pathModule.resolve(previous, linkTarget);

      if (cache) cache.set(base, resolvedLink);
      if (!isWindows) seenLinks[id] = linkTarget;
    }

    // resolve the link, then start over
    p = pathModule.resolve(resolvedLink, p.slice(pos));

    // Skip over roots
    current = base = splitRoot(p);
    pos = current.length;

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      const ctx = { path: base };
      binding.lstat(pathModule.toNamespacedPath(base), undefined, ctx);
      handleErrorFromBinding(ctx);
      knownHard[base] = true;
    }
  }

  if (cache) cache.set(original, p);
  return encodeRealpathResult(p, options);
};


fs.realpathSync.native = function(path, options) {
  options = getOptions(options, {});
  path = getPathFromURL(path);
  validatePath(path);
  const ctx = { path };
  const result = binding.realpath(path, options.encoding, undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
};


fs.realpath = function realpath(p, options, callback) {
  callback = maybeCallback(typeof options === 'function' ? options : callback);
  if (!options)
    options = emptyObj;
  else
    options = getOptions(options, emptyObj);
  p = getPathFromURL(p);
  if (typeof p !== 'string') {
    p += '';
  }
  validatePath(p);
  p = pathModule.resolve(p);

  const seenLinks = Object.create(null);
  const knownHard = Object.create(null);

  // current character position in p
  var pos;
  // the partial path so far, including a trailing slash if any
  var current;
  // the partial path without a trailing slash (except when pointing at a root)
  var base;
  // the partial path scanned in the previous round, with slash
  var previous;

  current = base = splitRoot(p);
  pos = current.length;

  // On windows, check that the root exists. On unix there is no need.
  if (isWindows && !knownHard[base]) {
    fs.lstat(base, function(err) {
      if (err) return callback(err);
      knownHard[base] = true;
      LOOP();
    });
  } else {
    process.nextTick(LOOP);
  }

  // walk down the path, swapping out linked path parts for their real
  // values
  function LOOP() {
    // stop if scanned past end of path
    if (pos >= p.length) {
      return callback(null, encodeRealpathResult(p, options));
    }

    // find the next part
    var result = nextPart(p, pos);
    previous = current;
    if (result === -1) {
      var last = p.slice(pos);
      current += last;
      base = previous + last;
      pos = p.length;
    } else {
      current += p.slice(pos, result + 1);
      base = previous + p.slice(pos, result);
      pos = result + 1;
    }

    // continue if not a symlink, break if a pipe/socket
    if (knownHard[base]) {
      if (isFileType(S_IFIFO) || isFileType(S_IFSOCK)) {
        return callback(null, encodeRealpathResult(p, options));
      }
      return process.nextTick(LOOP);
    }

    return fs.lstat(base, gotStat);
  }

  function gotStat(err) {
    if (err) return callback(err);

    // Use stats array directly to avoid creating an fs.Stats instance just for
    // our internal use.

    // if not a symlink, skip to the next path part
    if (!isFileType(S_IFLNK)) {
      knownHard[base] = true;
      return process.nextTick(LOOP);
    }

    // stat & read the link if not read before
    // call gotTarget as soon as the link target is known
    // dev/ino always return 0 on windows, so skip the check.
    let id;
    if (!isWindows) {
      var dev = statValues[0].toString(32);
      var ino = statValues[7].toString(32);
      id = `${dev}:${ino}`;
      if (seenLinks[id]) {
        return gotTarget(null, seenLinks[id], base);
      }
    }
    fs.stat(base, function(err) {
      if (err) return callback(err);

      fs.readlink(base, function(err, target) {
        if (!isWindows) seenLinks[id] = target;
        gotTarget(err, target);
      });
    });
  }

  function gotTarget(err, target, base) {
    if (err) return callback(err);

    var resolvedLink = pathModule.resolve(previous, target);
    gotResolvedLink(resolvedLink);
  }

  function gotResolvedLink(resolvedLink) {
    // resolve the link, then start over
    p = pathModule.resolve(resolvedLink, p.slice(pos));
    current = base = splitRoot(p);
    pos = current.length;

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      fs.lstat(base, function(err) {
        if (err) return callback(err);
        knownHard[base] = true;
        LOOP();
      });
    } else {
      process.nextTick(LOOP);
    }
  }
};


fs.realpath.native = function(path, options, callback) {
  callback = makeCallback(callback || options);
  options = getOptions(options, {});
  path = getPathFromURL(path);
  validatePath(path);
  const req = new FSReqWrap();
  req.oncomplete = callback;
  return binding.realpath(path, options.encoding, req);
};

fs.mkdtemp = function(prefix, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  if (!prefix || typeof prefix !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('prefix', 'string', prefix);
  }
  nullCheck(prefix, 'prefix');
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.mkdtemp(`${prefix}XXXXXX`, options.encoding, req);
};


fs.mkdtempSync = function(prefix, options) {
  options = getOptions(options, {});
  if (!prefix || typeof prefix !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('prefix', 'string', prefix);
  }
  nullCheck(prefix, 'prefix');
  const path = `${prefix}XXXXXX`;
  const ctx = { path };
  const result = binding.mkdtemp(path, options.encoding,
                                 undefined, ctx);
  handleErrorFromBinding(ctx);
  return result;
};


// Define copyFile() flags.
Object.defineProperties(fs.constants, {
  COPYFILE_EXCL: { enumerable: true, value: constants.UV_FS_COPYFILE_EXCL }
});


fs.copyFile = function(src, dest, flags, callback) {
  if (typeof flags === 'function') {
    callback = flags;
    flags = 0;
  } else if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK();
  }

  src = getPathFromURL(src);
  dest = getPathFromURL(dest);
  validatePath(src, 'src');
  validatePath(dest, 'dest');

  src = pathModule._makeLong(src);
  dest = pathModule._makeLong(dest);
  flags = flags | 0;
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.copyFile(src, dest, flags, req);
};


fs.copyFileSync = function(src, dest, flags) {
  src = getPathFromURL(src);
  dest = getPathFromURL(dest);
  validatePath(src, 'src');
  validatePath(dest, 'dest');

  const ctx = { path: src, dest };  // non-prefixed

  src = pathModule._makeLong(src);
  dest = pathModule._makeLong(dest);
  flags = flags | 0;
  binding.copyFile(src, dest, flags, undefined, ctx);
  handleErrorFromBinding(ctx);
};


var pool;

function allocNewPool(poolSize) {
  pool = Buffer.allocUnsafe(poolSize);
  pool.used = 0;
}


fs.createReadStream = function(path, options) {
  return new ReadStream(path, options);
};

util.inherits(ReadStream, Readable);
fs.ReadStream = ReadStream;

function ReadStream(path, options) {
  if (!(this instanceof ReadStream))
    return new ReadStream(path, options);

  // a little bit bigger buffer and water marks by default
  options = copyObject(getOptions(options, {}));
  if (options.highWaterMark === undefined)
    options.highWaterMark = 64 * 1024;

  // for backwards compat do not emit close on destroy.
  options.emitClose = false;

  Readable.call(this, options);

  // path will be ignored when fd is specified, so it can be falsy
  this.path = getPathFromURL(path);
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'r' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.end = options.end;
  this.autoClose = options.autoClose === undefined ? true : options.autoClose;
  this.pos = undefined;
  this.bytesRead = 0;
  this.closed = false;

  if (this.start !== undefined) {
    if (!Number.isInteger(this.start)) {
      throw new ERR_INVALID_ARG_TYPE('start', 'integer', this.start);
    }
    if (this.end === undefined) {
      this.end = Infinity;
    } else if (!Number.isInteger(this.end)) {
      throw new ERR_INVALID_ARG_TYPE('end', 'integer', this.end);
    }

    if (this.start > this.end) {
      const errVal = `{start: ${this.start}, end: ${this.end}}`;
      throw new ERR_OUT_OF_RANGE('start', '<= "end"', errVal);
    }

    this.pos = this.start;
  } else {
    // Backwards compatibility: Make sure `end` is a number regardless of
    // `start`.
    // TODO(addaleax): Make the above typecheck not depend on `start` instead.
    // (That is a semver-major change).
    if (typeof this.end !== 'number')
      this.end = Infinity;
    else if (!Number.isInteger(this.end))
      throw new ERR_INVALID_ARG_TYPE('end', 'integer', this.end);

    if (typeof this.fd !== 'number')
      this.open();

    this.on('end', function() {
      if (this.autoClose) {
        this.destroy();
      }
    });
  }
}

fs.FileReadStream = fs.ReadStream; // support the legacy name

ReadStream.prototype.open = function() {
  var self = this;
  fs.open(this.path, this.flags, this.mode, function(er, fd) {
    if (er) {
      if (self.autoClose) {
        self.destroy();
      }
      self.emit('error', er);
      return;
    }

    self.fd = fd;
    self.emit('open', fd);
    self.emit('ready');
    // start the flow of data.
    self.read();
  });
};

ReadStream.prototype._read = function(n) {
  if (typeof this.fd !== 'number') {
    return this.once('open', function() {
      this._read(n);
    });
  }

  if (this.destroyed)
    return;

  if (!pool || pool.length - pool.used < kMinPoolSpace) {
    // discard the old pool.
    allocNewPool(this.readableHighWaterMark);
  }

  // Grab another reference to the pool in the case that while we're
  // in the thread pool another read() finishes up the pool, and
  // allocates a new one.
  var thisPool = pool;
  var toRead = Math.min(pool.length - pool.used, n);
  var start = pool.used;

  if (this.pos !== undefined)
    toRead = Math.min(this.end - this.pos + 1, toRead);
  else
    toRead = Math.min(this.end - this.bytesRead + 1, toRead);

  // already read everything we were supposed to read!
  // treat as EOF.
  if (toRead <= 0)
    return this.push(null);

  // the actual read.
  fs.read(this.fd, pool, pool.used, toRead, this.pos, (er, bytesRead) => {
    if (er) {
      if (this.autoClose) {
        this.destroy();
      }
      this.emit('error', er);
    } else {
      var b = null;
      if (bytesRead > 0) {
        this.bytesRead += bytesRead;
        b = thisPool.slice(start, start + bytesRead);
      }

      this.push(b);
    }
  });

  // move the pool positions, and internal position for reading.
  if (this.pos !== undefined)
    this.pos += toRead;
  pool.used += toRead;
};

ReadStream.prototype._destroy = function(err, cb) {
  const isOpen = typeof this.fd !== 'number';
  if (isOpen) {
    this.once('open', closeFsStream.bind(null, this, cb, err));
    return;
  }

  closeFsStream(this, cb);
  this.fd = null;
};

function closeFsStream(stream, cb, err) {
  fs.close(stream.fd, (er) => {
    er = er || err;
    cb(er);
    stream.closed = true;
    if (!er)
      stream.emit('close');
  });
}

ReadStream.prototype.close = function(cb) {
  this.destroy(null, cb);
};

fs.createWriteStream = function(path, options) {
  return new WriteStream(path, options);
};

util.inherits(WriteStream, Writable);
fs.WriteStream = WriteStream;
function WriteStream(path, options) {
  if (!(this instanceof WriteStream))
    return new WriteStream(path, options);

  options = copyObject(getOptions(options, {}));

  // for backwards compat do not emit close on destroy.
  options.emitClose = false;

  Writable.call(this, options);

  // path will be ignored when fd is specified, so it can be falsy
  this.path = getPathFromURL(path);
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'w' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.autoClose = options.autoClose === undefined ? true : !!options.autoClose;
  this.pos = undefined;
  this.bytesWritten = 0;
  this.closed = false;

  if (this.start !== undefined) {
    if (typeof this.start !== 'number') {
      throw new ERR_INVALID_ARG_TYPE('start', 'number', this.start);
    }
    if (this.start < 0) {
      const errVal = `{start: ${this.start}}`;
      throw new ERR_OUT_OF_RANGE('start', '>= 0', errVal);
    }

    this.pos = this.start;
  }

  if (options.encoding)
    this.setDefaultEncoding(options.encoding);

  if (typeof this.fd !== 'number')
    this.open();

  // dispose on finish.
  this.once('finish', function() {
    if (this.autoClose) {
      this.destroy();
    }
  });
}

fs.FileWriteStream = fs.WriteStream; // support the legacy name


WriteStream.prototype.open = function() {
  fs.open(this.path, this.flags, this.mode, (er, fd) => {
    if (er) {
      if (this.autoClose) {
        this.destroy();
      }
      this.emit('error', er);
      return;
    }

    this.fd = fd;
    this.emit('open', fd);
    this.emit('ready');
  });
};


WriteStream.prototype._write = function(data, encoding, cb) {
  if (!(data instanceof Buffer)) {
    const err = new ERR_INVALID_ARG_TYPE('data', 'Buffer', data);
    return this.emit('error', err);
  }

  if (typeof this.fd !== 'number') {
    return this.once('open', function() {
      this._write(data, encoding, cb);
    });
  }

  fs.write(this.fd, data, 0, data.length, this.pos, (er, bytes) => {
    if (er) {
      if (this.autoClose) {
        this.destroy();
      }
      return cb(er);
    }
    this.bytesWritten += bytes;
    cb();
  });

  if (this.pos !== undefined)
    this.pos += data.length;
};


function writev(fd, chunks, position, callback) {
  function wrapper(err, written) {
    // Retain a reference to chunks so that they can't be GC'ed too soon.
    callback(err, written || 0, chunks);
  }

  const req = new FSReqWrap();
  req.oncomplete = wrapper;
  binding.writeBuffers(fd, chunks, position, req);
}


WriteStream.prototype._writev = function(data, cb) {
  if (typeof this.fd !== 'number') {
    return this.once('open', function() {
      this._writev(data, cb);
    });
  }

  const self = this;
  const len = data.length;
  const chunks = new Array(len);
  var size = 0;

  for (var i = 0; i < len; i++) {
    var chunk = data[i].chunk;

    chunks[i] = chunk;
    size += chunk.length;
  }

  writev(this.fd, chunks, this.pos, function(er, bytes) {
    if (er) {
      self.destroy();
      return cb(er);
    }
    self.bytesWritten += bytes;
    cb();
  });

  if (this.pos !== undefined)
    this.pos += size;
};


WriteStream.prototype._destroy = ReadStream.prototype._destroy;
WriteStream.prototype.close = function(cb) {
  if (cb) {
    if (this.closed) {
      process.nextTick(cb);
      return;
    } else {
      this.on('close', cb);
    }
  }

  // If we are not autoClosing, we should call
  // destroy on 'finish'.
  if (!this.autoClose) {
    this.on('finish', this.destroy.bind(this));
  }

  // we use end() instead of destroy() because of
  // https://github.com/nodejs/node/issues/2006
  this.end();
};

// There is no shutdown() for files.
WriteStream.prototype.destroySoon = WriteStream.prototype.end;

// SyncWriteStream is internal. DO NOT USE.
// This undocumented API was never intended to be made public.
var SyncWriteStream = internalFS.SyncWriteStream;
Object.defineProperty(fs, 'SyncWriteStream', {
  configurable: true,
  get: internalUtil.deprecate(() => SyncWriteStream,
                              'fs.SyncWriteStream is deprecated.', 'DEP0061'),
  set: internalUtil.deprecate((val) => { SyncWriteStream = val; },
                              'fs.SyncWriteStream is deprecated.', 'DEP0061')
});
