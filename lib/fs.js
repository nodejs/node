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
const { isUint8Array, createPromise, promiseResolve } = process.binding('util');

const binding = process.binding('fs');
const fs = exports;
const Buffer = require('buffer').Buffer;
const errors = require('internal/errors');
const Stream = require('stream').Stream;
const EventEmitter = require('events');
const FSReqWrap = binding.FSReqWrap;
const FSEvent = process.binding('fs_event_wrap').FSEvent;
const internalURL = require('internal/url');
const internalUtil = require('internal/util');
const getPathFromURL = internalURL.getPathFromURL;

const {
  assertEncoding,
  getOptions,
  modeNum,
  preprocessSymlinkDestination,
  realpathCacheKey,
  stringToFlags,
  stringToSymlinkType,
  toUnixTimestamp,
  statsFromValues,
  statsFromPrevValues,
  statValues,
  Stats,
  SyncWriteStream: _SyncWriteStream
} = require('internal/fs');

Object.defineProperty(exports, 'constants', {
  configurable: false,
  enumerable: true,
  value: constants
});

const Readable = Stream.Readable;
const Writable = Stream.Writable;

const kMinPoolSpace = 128;
const kMaxLength = require('buffer').kMaxLength;

const isWindows = process.platform === 'win32';

const DEBUG = process.env.NODE_DEBUG && /fs/.test(process.env.NODE_DEBUG);
const errnoException = util._errnoException;

function copyObject(source) {
  var target = {};
  for (var key in source)
    target[key] = source[key];
  return target;
}

function rethrow() {
  // TODO(thefourtheye) Throw error instead of warning in major version > 7
  process.emitWarning(
    'Calling an asynchronous function without callback is deprecated.',
    'DeprecationWarning', 'DEP0013', rethrow
  );

  // Only enable in debug mode. A backtrace uses ~1000 bytes of heap space and
  // is fairly slow to generate.
  if (DEBUG) {
    var backtrace = new Error();
    return function(err) {
      if (err) {
        backtrace.stack = err.name + ': ' + err.message +
                          backtrace.stack.substr(backtrace.name.length);
        throw backtrace;
      }
    };
  }

  return function(err) {
    if (err) {
      throw err;  // Forgot a callback but don't know where? Use NODE_DEBUG=fs
    }
  };
}

function maybeCallback(cb) {
  return typeof cb === 'function' ? cb : rethrow();
}

// Ensure that callbacks run in the global context. Only use this function
// for callbacks that are passed to the binding layer, callbacks that are
// invoked from JS already run in the proper scope.
function makeCallback(cb) {
  if (cb === undefined) {
    return rethrow();
  }

  if (typeof cb !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }

  return function() {
    return cb.apply(undefined, arguments);
  };
}

// Special case of `makeCallback()` that is specific to async `*stat()` calls as
// an optimization, since the data passed back to the callback needs to be
// transformed anyway.
function makeStatsCallback(cb) {
  if (cb === undefined) {
    return rethrow();
  }

  if (typeof cb !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }

  return function(err) {
    if (err) return cb(err);
    cb(err, statsFromValues());
  };
}

function nullCheck(path, callback) {
  if (('' + path).indexOf('\u0000') !== -1) {
    const er = new errors.Error('ERR_INVALID_ARG_TYPE',
                                'path',
                                'string without null bytes',
                                path);

    if (typeof callback !== 'function')
      throw er;
    process.nextTick(callback, er);
    return false;
  }
  return true;
}

function isFd(path) {
  return (path >>> 0) === path;
}

fs.Stats = Stats;

// Don't allow mode to accidentally be overwritten.
Object.defineProperties(fs, {
  F_OK: { enumerable: true, value: constants.F_OK || 0 },
  R_OK: { enumerable: true, value: constants.R_OK || 0 },
  W_OK: { enumerable: true, value: constants.W_OK || 0 },
  X_OK: { enumerable: true, value: constants.X_OK || 0 },
});

function handleError(val) {
  if (val instanceof Error)
    throw val;
}

fs.access = function(path, mode, callback) {
  if (typeof mode === 'function') {
    callback = mode;
    mode = fs.F_OK;
  } else if (typeof callback !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }

  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  if (!Number.isInteger(mode)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'mode', 'integer');
  }

  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.access(pathModule._makeLong(path), mode, req);
};

fs.accessSync = function(path, mode = fs.F_OK) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  if (!Number.isInteger(mode)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'mode', 'integer');
  }

  binding.access(pathModule._makeLong(path), mode);
};

fs.exists = function(path, callback) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  const req = new FSReqWrap();
  req.oncomplete = cb;
  binding.stat(pathModule._makeLong(path), req);
  function cb(err) {
    if (callback) callback(err ? false : true);
  }
};

Object.defineProperty(fs.exists, internalUtil.promisify.custom, {
  value: (path) => {
    const promise = createPromise();
    fs.exists(path, (exists) => promiseResolve(promise, exists));
    return promise;
  }
});


fs.existsSync = function(path) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  try {
    binding.stat(pathModule._makeLong(path));
    return true;
  } catch (e) {
    return false;
  }
};

fs.readFile = function(path, options, callback) {
  callback = maybeCallback(callback || options);
  options = getOptions(options, { flag: 'r' });

  handleError(path = getPathFromURL(path));
  nullCheck(path);

  const context = new ReadFileContext(callback, options.encoding);
  context.isUserFd = isFd(path); // file descriptor ownership
  const req = new FSReqWrap();
  req.context = context;
  req.oncomplete = readFileAfterOpen;

  if (context.isUserFd) {
    process.nextTick(function() {
      req.oncomplete(null, path);
    });
    return;
  }

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  binding.open(pathModule._makeLong(path),
               stringToFlags(options.flag || 'r', true),
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
    length = this.size - this.pos;
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

  // Use stats array directly to avoid creating an fs.Stats instance just for
  // our internal use.
  var size;
  if ((statValues[1/*mode*/] & S_IFMT) === S_IFREG)
    size = context.size = statValues[8/*size*/];
  else
    size = context.size = 0;

  if (size === 0) {
    context.buffers = [];
    context.read();
    return;
  }

  if (size > kMaxLength) {
    err = new RangeError('File size is greater than possible Buffer: ' +
                         `0x${kMaxLength.toString(16)} bytes`);
    return context.close(err);
  }

  context.buffer = Buffer.allocUnsafeSlow(size);
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

  if (context.size === 0)
    buffer = Buffer.concat(context.buffers, context.pos);
  else if (context.pos < context.size)
    buffer = context.buffer.slice(0, context.pos);
  else
    buffer = context.buffer;

  if (context.encoding) {
    return tryToString(buffer, context.encoding, callback);
  }

  callback(null, buffer);
}

function tryToString(buf, encoding, callback) {
  try {
    buf = buf.toString(encoding);
  } catch (err) {
    return callback(err);
  }
  callback(null, buf);
}

function tryStatSync(fd, isUserFd) {
  var threw = true;
  try {
    binding.fstat(fd);
    threw = false;
  } finally {
    if (threw && !isUserFd) fs.closeSync(fd);
  }
}

function tryCreateBuffer(size, fd, isUserFd) {
  var threw = true;
  var buffer;
  try {
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
  // Use stats array directly to avoid creating an fs.Stats instance just for
  // our internal use.
  var size;
  if ((statValues[1/*mode*/] & S_IFMT) === S_IFREG)
    size = statValues[8/*size*/];
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


// Yes, the follow could be easily DRYed up but I provide the explicit
// list to make the arguments clear.

fs.close = function(fd, callback) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.close(fd, req);
};

fs.closeSync = function(fd) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  return binding.close(fd);
};

fs.open = function(path, flags, mode, callback_) {
  const callback = makeCallback(arguments[arguments.length - 1]);
  mode = modeNum(mode, 0o666);
  flags = stringToFlags(flags);

  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.open(pathModule._makeLong(path), flags, mode, req);
};

fs.openSync = function(path, flags, mode) {
  mode = modeNum(mode, 0o666);
  flags = stringToFlags(flags);
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  return binding.open(pathModule._makeLong(path), flags, mode);
};

fs.read = function(fd, buffer, offset, length, position, callback) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  if (!isUint8Array(buffer)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'buffer',
                               ['Buffer', 'Uint8Array']);
  }
  if (length === 0) {
    return process.nextTick(function() {
      callback && callback(null, 0, buffer);
    });
  }

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
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  if (!isUint8Array(buffer)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'buffer',
                               ['Buffer', 'Uint8Array']);
  }
  if (length === 0) {
    return 0;
  }

  return binding.read(fd, buffer, offset, length, position);
};

// usage:
//  fs.write(fd, buffer[, offset[, length[, position]]], callback);
// OR
//  fs.write(fd, string[, position[, encoding]], callback);
fs.write = function(fd, buffer, offset, length, position, callback) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  function wrapper(err, written) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback(err, written || 0, buffer);
  }

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

    if (offset > buffer.length)
      throw new errors.RangeError('ERR_OUTOFBOUNDS', 'offset');
    if (length > buffer.length)
      throw new errors.RangeError('ERR_OUTOFBOUNDS', 'length');
    if (offset + length < offset)
      throw new errors.RangeError('ERR_OVERFLOW');

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
  assertEncoding(length);
  return binding.writeString(fd, buffer, offset, length, req);
};

Object.defineProperty(fs.write, internalUtil.customPromisifyArgs,
                      { value: ['bytesWritten', 'buffer'], enumerable: false });

// usage:
//  fs.writeSync(fd, buffer[, offset[, length[, position]]]);
// OR
//  fs.writeSync(fd, string[, position[, encoding]]);
fs.writeSync = function(fd, buffer, offset, length, position) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  if (isUint8Array(buffer)) {
    if (position === undefined)
      position = null;
    if (typeof offset !== 'number')
      offset = 0;
    if (typeof length !== 'number')
      length = buffer.length - offset;

    if (offset > buffer.length)
      throw new errors.RangeError('ERR_OUTOFBOUNDS', 'offset');
    if (length > buffer.length)
      throw new errors.RangeError('ERR_OUTOFBOUNDS', 'length');
    if (offset + length < offset)
      throw new errors.RangeError('ERR_OVERFLOW');

    return binding.writeBuffer(fd, buffer, offset, length, position);
  }
  if (typeof buffer !== 'string')
    buffer += '';
  if (offset === undefined)
    offset = null;
  assertEncoding(length);
  return binding.writeString(fd, buffer, offset, length);
};

fs.rename = function(oldPath, newPath, callback) {
  callback = makeCallback(callback);
  handleError(oldPath = getPathFromURL(oldPath));
  handleError(newPath = getPathFromURL(newPath));

  nullCheck(oldPath);
  nullCheck(newPath);

  if (typeof oldPath !== 'string' && !Buffer.isBuffer(oldPath)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'oldPath',
                               ['string', 'Buffer', 'URL']);
  }

  if (typeof newPath !== 'string' && !Buffer.isBuffer(newPath)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'newPath',
                               ['string', 'Buffer', 'URL']);
  }

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.rename(pathModule._makeLong(oldPath),
                 pathModule._makeLong(newPath),
                 req);
};

fs.renameSync = function(oldPath, newPath) {
  handleError(oldPath = getPathFromURL(oldPath));
  handleError(newPath = getPathFromURL(newPath));
  nullCheck(oldPath);
  nullCheck(newPath);

  if (typeof oldPath !== 'string' && !Buffer.isBuffer(oldPath)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'oldPath',
                               ['string', 'Buffer', 'URL']);
  }

  if (typeof newPath !== 'string' && !Buffer.isBuffer(newPath)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'newPath',
                               ['string', 'Buffer', 'URL']);
  }

  return binding.rename(pathModule._makeLong(oldPath),
                        pathModule._makeLong(newPath));
};

fs.truncate = function(path, len, callback) {
  if (typeof path === 'number') {
    return fs.ftruncate(path, len, callback);
  }
  if (typeof len === 'function') {
    callback = len;
    len = 0;
  } else if (len == null) {
    len = 0;
  }

  callback = maybeCallback(callback);
  fs.open(path, 'r+', function(er, fd) {
    if (er) return callback(er);
    const req = new FSReqWrap();
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
    return fs.ftruncateSync(path, len);
  }
  if (len == null) {
    len = 0;
  }
  // allow error to be thrown, but still close fd.
  const fd = fs.openSync(path, 'r+');
  var ret;

  try {
    ret = fs.ftruncateSync(fd, len);
  } finally {
    fs.closeSync(fd);
  }
  return ret;
};

fs.ftruncate = function(fd, len, callback) {
  if (typeof len === 'function') {
    callback = len;
    len = 0;
  } else if (len == null) {
    len = 0;
  }
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }

  if (!Number.isInteger(len) || len < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'len',
                               'unsigned integer');
  }
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.ftruncate(fd, len, req);
};

fs.ftruncateSync = function(fd, len) {
  if (len == null)
    len = 0;
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }

  if (!Number.isInteger(len) || len < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'len',
                               'unsigned integer');
  }
  return binding.ftruncate(fd, len);
};

fs.rmdir = function(path, callback) {
  callback = maybeCallback(callback);
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.rmdir(pathModule._makeLong(path), req);
};

fs.rmdirSync = function(path) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  return binding.rmdir(pathModule._makeLong(path));
};

fs.fdatasync = function(fd, callback) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }

  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fdatasync(fd, req);
};

fs.fdatasyncSync = function(fd) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }

  return binding.fdatasync(fd);
};

fs.fsync = function(fd, callback) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fsync(fd, req);
};

fs.fsyncSync = function(fd) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  return binding.fsync(fd);
};

fs.mkdir = function(path, mode, callback) {
  if (typeof mode === 'function') callback = mode;
  callback = makeCallback(callback);
  mode = modeNum(0o777);
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.mkdir(pathModule._makeLong(path), mode, req);
};

fs.mkdirSync = function(path, mode) {
  mode = modeNum(0o777);
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  return binding.mkdir(pathModule._makeLong(path), mode);
};

fs.readdir = function(path, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readdir(pathModule._makeLong(path), options.encoding, req);
};

fs.readdirSync = function(path, options) {
  options = getOptions(options, {});
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  return binding.readdir(pathModule._makeLong(path), options.encoding);
};

fs.fstat = function(fd, callback) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  const req = new FSReqWrap();
  req.oncomplete = makeStatsCallback(callback);
  binding.fstat(fd, req);
};

fs.lstat = function(path, callback) {
  callback = makeStatsCallback(callback);
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.lstat(pathModule._makeLong(path), req);
};

fs.stat = function(path, callback) {
  callback = makeStatsCallback(callback);
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.stat(pathModule._makeLong(path), req);
};

fs.fstatSync = function(fd) {
  binding.fstat(fd);
  return statsFromValues();
};

fs.lstatSync = function(path) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  binding.lstat(pathModule._makeLong(path));
  return statsFromValues();
};

fs.statSync = function(path) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  binding.stat(pathModule._makeLong(path));
  return statsFromValues();
};

fs.readlink = function(path, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readlink(pathModule._makeLong(path), options.encoding, req);
};

fs.readlinkSync = function(path, options) {
  options = getOptions(options, {});
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  return binding.readlink(pathModule._makeLong(path), options.encoding);
};

fs.symlink = function(target, path, type, callback) {
  if (typeof type === 'function') {
    callback = type;
    type = 'file';
  }
  const _type = stringToSymlinkType(type);

  handleError(target = getPathFromURL(target));
  handleError(path = getPathFromURL(path));

  nullCheck(target);
  nullCheck(path);

  if (typeof target !== 'string' && !Buffer.isBuffer(target)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'target',
                               ['string', 'Buffer', 'URL']);
  }

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);

  binding.symlink(preprocessSymlinkDestination(target, type, path),
                  pathModule._makeLong(path),
                  _type,
                  req);
};

fs.symlinkSync = function(target, path, type = 'file') {
  const _type = stringToSymlinkType(type);
  handleError(target = getPathFromURL(target));
  handleError(path = getPathFromURL(path));
  nullCheck(target);
  nullCheck(path);

  if (typeof target !== 'string' && !Buffer.isBuffer(target)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'target',
                               ['string', 'Buffer', 'URL']);
  }

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  return binding.symlink(preprocessSymlinkDestination(target, type, path),
                         pathModule._makeLong(path),
                         _type);
};

fs.link = function(existingPath, newPath, callback) {
  callback = makeCallback(callback);

  handleError(existingPath = getPathFromURL(existingPath));
  handleError(newPath = getPathFromURL(newPath));

  nullCheck(existingPath);
  nullCheck(newPath);

  if (typeof existingPath !== 'string' && !Buffer.isBuffer(existingPath)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'src',
                               ['string', 'Buffer', 'URL']);
  }

  if (typeof newPath !== 'string' && !Buffer.isBuffer(newPath)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'dest',
                               ['string', 'Buffer', 'URL']);
  }
  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.link(pathModule._makeLong(existingPath),
               pathModule._makeLong(newPath),
               req);
};

fs.linkSync = function(existingPath, newPath) {
  handleError(existingPath = getPathFromURL(existingPath));
  handleError(newPath = getPathFromURL(newPath));
  nullCheck(existingPath);
  nullCheck(newPath);
  if (typeof existingPath !== 'string' && !Buffer.isBuffer(existingPath)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'src',
                               ['string', 'Buffer', 'URL']);
  }

  if (typeof newPath !== 'string' && !Buffer.isBuffer(newPath)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'dest',
                               ['string', 'Buffer', 'URL']);
  }
  return binding.link(pathModule._makeLong(existingPath),
                      pathModule._makeLong(newPath));
};

fs.unlink = function(path, callback) {
  callback = makeCallback(callback);
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.unlink(pathModule._makeLong(path), req);
};

fs.unlinkSync = function(path) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  return binding.unlink(pathModule._makeLong(path));
};

fs.fchmod = function(fd, mode, callback) {
  mode = modeNum(mode);

  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }

  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fchmod(fd, mode, req);
};

fs.fchmodSync = function(fd, mode) {
  mode = modeNum(mode);

  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }
  return binding.fchmod(fd, mode);
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
    var ret;
    try {
      ret = fs.fchmodSync(fd, mode);
    } catch (err) {
      try {
        fs.closeSync(fd);
      } catch (ignore) {}
      throw err;
    }
    fs.closeSync(fd);
    return ret;
  };
}


fs.chmod = function(path, mode, callback) {
  callback = makeCallback(callback);
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  mode = modeNum(mode);
  if (!Number.isInteger(mode) || mode < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'mode',
                               'unsigned integer');
  }

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.chmod(pathModule._makeLong(path), mode, req);
};

fs.chmodSync = function(path, mode) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  mode = modeNum(mode);
  if (!Number.isInteger(mode) || mode < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'mode',
                               'unsigned integer');
  }

  return binding.chmod(pathModule._makeLong(path), mode);
};

if (constants.O_SYMLINK !== undefined) {
  fs.lchown = function(path, uid, gid, callback) {
    callback = maybeCallback(callback);
    fs.open(path, constants.O_WRONLY | constants.O_SYMLINK, function(err, fd) {
      if (err) {
        callback(err);
        return;
      }
      fs.fchown(fd, uid, gid, callback);
    });
  };

  fs.lchownSync = function(path, uid, gid) {
    var fd = fs.openSync(path, constants.O_WRONLY | constants.O_SYMLINK);
    return fs.fchownSync(fd, uid, gid);
  };
}

fs.fchown = function(fd, uid, gid, callback) {

  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }

  if (!Number.isInteger(uid)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'uid', 'integer');
  }

  if (!Number.isInteger(gid)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'gid', 'integer');
  }

  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fchown(fd, uid, gid, req);
};

fs.fchownSync = function(fd, uid, gid) {

  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }

  if (!Number.isInteger(uid)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'uid', 'integer');
  }

  if (!Number.isInteger(gid)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'gid', 'integer');
  }

  return binding.fchown(fd, uid, gid);
};

fs.chown = function(path, uid, gid, callback) {
  callback = makeCallback(callback);
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  if (!Number.isInteger(uid) || uid < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'uid',
                               'unsigned integer');
  }

  if (!Number.isInteger(gid)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'gid', 'integer');
  }

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.chown(pathModule._makeLong(path), uid, gid, req);
};

fs.chownSync = function(path, uid, gid) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);

  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  if (!Number.isInteger(uid)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'uid', 'integer');
  }

  if (!Number.isInteger(gid)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'gid', 'integer');
  }

  return binding.chown(pathModule._makeLong(path), uid, gid);
};

// exported for unit tests, not for public consumption
fs._toUnixTimestamp = toUnixTimestamp;

fs.utimes = function(path, atime, mtime, callback) {
  callback = makeCallback(callback);
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.utimes(pathModule._makeLong(path),
                 toUnixTimestamp(atime),
                 toUnixTimestamp(mtime),
                 req);
};

fs.utimesSync = function(path, atime, mtime) {
  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }
  atime = toUnixTimestamp(atime);
  mtime = toUnixTimestamp(mtime);
  binding.utimes(pathModule._makeLong(path), atime, mtime);
};

fs.futimes = function(fd, atime, mtime, callback) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }

  atime = toUnixTimestamp(atime);
  mtime = toUnixTimestamp(mtime);
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.futimes(fd, atime, mtime, req);
};

fs.futimesSync = function(fd, atime, mtime) {
  if (!Number.isInteger(fd) || fd < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                               'unsigned integer');
  }

  atime = toUnixTimestamp(atime);
  mtime = toUnixTimestamp(mtime);
  binding.futimes(fd, atime, mtime);
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
    } else {
      if (written === length) {
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

  this._handle = new FSEvent();
  this._handle.owner = this;

  this._handle.onchange = (status, eventType, filename) => {
    if (status < 0) {
      this._handle.close();
      const error = !filename ?
        errnoException(status, 'Error watching file for changes:') :
        errnoException(status, `Error watching file ${filename} for changes:`);
      error.filename = filename;
      this.emit('error', error);
    } else {
      this.emit('change', eventType, filename);
    }
  };
}
util.inherits(FSWatcher, EventEmitter);

FSWatcher.prototype.start = function(filename,
                                     persistent,
                                     recursive,
                                     encoding) {
  handleError(filename = getPathFromURL(filename));
  nullCheck(filename);
  var err = this._handle.start(pathModule._makeLong(filename),
                               persistent,
                               recursive,
                               encoding);
  if (err) {
    this._handle.close();
    const error = errnoException(err, `watch ${filename}`);
    error.filename = filename;
    throw error;
  }
};

FSWatcher.prototype.close = function() {
  this._handle.close();
};

fs.watch = function(filename, options, listener) {
  handleError(filename = getPathFromURL(filename));
  nullCheck(filename);

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
        statValues[2/*new nlink*/] === statValues[16/*old nlink*/]) return;

    oldStatus = newStatus;
    self.emit('change', statsFromValues(), statsFromPrevValues());
  };

  this._handle.onstop = function() {
    process.nextTick(emitStop, self);
  };
}
util.inherits(StatWatcher, EventEmitter);


StatWatcher.prototype.start = function(filename, persistent, interval) {
  handleError(filename = getPathFromURL(filename));
  nullCheck(filename);
  this._handle.start(pathModule._makeLong(filename), persistent, interval);
};


StatWatcher.prototype.stop = function() {
  this._handle.stop();
};


const statWatchers = new Map();

fs.watchFile = function(filename, options, listener) {
  handleError(filename = getPathFromURL(filename));
  nullCheck(filename);
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
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'listener',
                               'function',
                               listener);
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
  handleError(filename = getPathFromURL(filename));
  nullCheck(filename);
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
      if (str.charCodeAt(i) !== 47/*'/'*/)
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
      if (ch === 92/*'\'*/ || ch === 47/*'/'*/)
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
  if (typeof p !== 'string') {
    handleError((p = getPathFromURL(p)));
    if (typeof p !== 'string')
      p += '';
  }
  nullCheck(p);
  p = pathModule.resolve(p);

  const cache = options[realpathCacheKey];
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
    binding.lstat(pathModule._makeLong(base));
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
      if ((statValues[1/*mode*/] & S_IFMT) === S_IFIFO ||
          (statValues[1/*mode*/] & S_IFMT) === S_IFSOCK) {
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

      var baseLong = pathModule._makeLong(base);
      binding.lstat(baseLong);

      if ((statValues[1/*mode*/] & S_IFMT) !== S_IFLNK) {
        knownHard[base] = true;
        if (cache) cache.set(base, base);
        continue;
      }

      // read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      var linkTarget = null;
      var id;
      if (!isWindows) {
        var dev = statValues[0/*dev*/].toString(32);
        var ino = statValues[7/*ino*/].toString(32);
        id = `${dev}:${ino}`;
        if (seenLinks[id]) {
          linkTarget = seenLinks[id];
        }
      }
      if (linkTarget === null) {
        binding.stat(baseLong);
        linkTarget = binding.readlink(baseLong);
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
      binding.lstat(pathModule._makeLong(base));
      knownHard[base] = true;
    }
  }

  if (cache) cache.set(original, p);
  return encodeRealpathResult(p, options);
};


fs.realpath = function realpath(p, options, callback) {
  callback = maybeCallback(typeof options === 'function' ? options : callback);
  if (!options)
    options = emptyObj;
  else
    options = getOptions(options, emptyObj);
  if (typeof p !== 'string') {
    handleError(p = getPathFromURL(p));
    if (typeof p !== 'string')
      p += '';
  }
  nullCheck(p);
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
      if ((statValues[1/*mode*/] & S_IFMT) === S_IFIFO ||
          (statValues[1/*mode*/] & S_IFMT) === S_IFSOCK) {
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
    if ((statValues[1/*mode*/] & S_IFMT) !== S_IFLNK) {
      knownHard[base] = true;
      return process.nextTick(LOOP);
    }

    // stat & read the link if not read before
    // call gotTarget as soon as the link target is known
    // dev/ino always return 0 on windows, so skip the check.
    let id;
    if (!isWindows) {
      var dev = statValues[0/*ino*/].toString(32);
      var ino = statValues[7/*ino*/].toString(32);
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

fs.mkdtemp = function(prefix, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  if (!prefix || typeof prefix !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'prefix',
                               'string',
                               prefix);
  }
  nullCheck(prefix);

  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.mkdtemp(prefix + 'XXXXXX', options.encoding, req);
};


fs.mkdtempSync = function(prefix, options) {
  if (!prefix || typeof prefix !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'prefix',
                               'string',
                               prefix);
  }
  options = getOptions(options, {});
  nullCheck(prefix);
  return binding.mkdtemp(prefix + 'XXXXXX', options.encoding);
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
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'callback', 'function');
  }

  src = getPathFromURL(src);

  handleError(src);
  nullCheck(src);

  if (typeof src !== 'string' && !Buffer.isBuffer(src)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'src',
                               ['string', 'Buffer', 'URL']);
  }

  dest = getPathFromURL(dest);

  handleError(dest);
  nullCheck(dest);

  if (typeof dest !== 'string' && !Buffer.isBuffer(dest)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'dest',
                               ['string', 'Buffer', 'URL']);
  }

  src = pathModule._makeLong(src);
  dest = pathModule._makeLong(dest);
  flags = flags | 0;
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.copyFile(src, dest, flags, req);
};


fs.copyFileSync = function(src, dest, flags) {
  src = getPathFromURL(src);
  handleError(src);
  nullCheck(src);

  if (typeof src !== 'string' && !Buffer.isBuffer(src)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'src',
                               ['string', 'Buffer', 'URL']);
  }

  dest = getPathFromURL(dest);
  handleError(dest);
  nullCheck(dest);

  if (typeof dest !== 'string' && !Buffer.isBuffer(dest)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'dest',
                               ['string', 'Buffer', 'URL']);
  }

  src = pathModule._makeLong(src);
  dest = pathModule._makeLong(dest);
  flags = flags | 0;
  binding.copyFile(src, dest, flags);
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

  Readable.call(this, options);

  handleError(this.path = getPathFromURL(path));
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'r' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.end = options.end;
  this.autoClose = options.autoClose === undefined ? true : options.autoClose;
  this.pos = undefined;
  this.bytesRead = 0;

  if (this.start !== undefined) {
    if (typeof this.start !== 'number') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'start',
                                 'number',
                                 this.start);
    }
    if (this.end === undefined) {
      this.end = Infinity;
    } else if (typeof this.end !== 'number') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'end',
                                 'number',
                                 this.end);
    }

    if (this.start > this.end) {
      const errVal = `{start: ${this.start}, end: ${this.end}}`;
      throw new errors.RangeError('ERR_VALUE_OUT_OF_RANGE',
                                  'start',
                                  '<= "end"',
                                  errVal);
    }

    this.pos = this.start;
  }

  if (typeof this.fd !== 'number')
    this.open();

  this.on('end', function() {
    if (this.autoClose) {
      this.destroy();
    }
  });
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
    allocNewPool(this._readableState.highWaterMark);
  }

  // Grab another reference to the pool in the case that while we're
  // in the thread pool another read() finishes up the pool, and
  // allocates a new one.
  var thisPool = pool;
  var toRead = Math.min(pool.length - pool.used, n);
  var start = pool.used;

  if (this.pos !== undefined)
    toRead = Math.min(this.end - this.pos + 1, toRead);

  // already read everything we were supposed to read!
  // treat as EOF.
  if (toRead <= 0)
    return this.push(null);

  // the actual read.
  var self = this;
  fs.read(this.fd, pool, pool.used, toRead, this.pos, onread);

  // move the pool positions, and internal position for reading.
  if (this.pos !== undefined)
    this.pos += toRead;
  pool.used += toRead;

  function onread(er, bytesRead) {
    if (er) {
      if (self.autoClose) {
        self.destroy();
      }
      self.emit('error', er);
    } else {
      var b = null;
      if (bytesRead > 0) {
        self.bytesRead += bytesRead;
        b = thisPool.slice(start, start + bytesRead);
      }

      self.push(b);
    }
  }
};


ReadStream.prototype._destroy = function(err, cb) {
  this.close(function(err2) {
    cb(err || err2);
  });
};


ReadStream.prototype.close = function(cb) {
  if (cb)
    this.once('close', cb);

  if (this.closed || typeof this.fd !== 'number') {
    if (typeof this.fd !== 'number') {
      this.once('open', closeOnOpen);
      return;
    }
    return process.nextTick(() => this.emit('close'));
  }

  this.closed = true;

  fs.close(this.fd, (er) => {
    if (er)
      this.emit('error', er);
    else
      this.emit('close');
  });

  this.fd = null;
};

// needed because as it will be called with arguments
// that does not match this.close() signature
function closeOnOpen(fd) {
  this.close();
}

fs.createWriteStream = function(path, options) {
  return new WriteStream(path, options);
};

util.inherits(WriteStream, Writable);
fs.WriteStream = WriteStream;
function WriteStream(path, options) {
  if (!(this instanceof WriteStream))
    return new WriteStream(path, options);

  options = copyObject(getOptions(options, {}));

  Writable.call(this, options);

  handleError(this.path = getPathFromURL(path));
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'w' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.autoClose = options.autoClose === undefined ? true : !!options.autoClose;
  this.pos = undefined;
  this.bytesWritten = 0;

  if (this.start !== undefined) {
    if (typeof this.start !== 'number') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'start',
                                 'number',
                                 this.start);
    }
    if (this.start < 0) {
      const errVal = `{start: ${this.start}}`;
      throw new errors.RangeError('ERR_VALUE_OUT_OF_RANGE',
                                  'start',
                                  '>= 0',
                                  errVal);
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
      this.close();
    }
  });
}

fs.FileWriteStream = fs.WriteStream; // support the legacy name


WriteStream.prototype.open = function() {
  fs.open(this.path, this.flags, this.mode, function(er, fd) {
    if (er) {
      if (this.autoClose) {
        this.destroy();
      }
      this.emit('error', er);
      return;
    }

    this.fd = fd;
    this.emit('open', fd);
  }.bind(this));
};


WriteStream.prototype._write = function(data, encoding, cb) {
  if (!(data instanceof Buffer))
    return this.emit('error', new Error('Invalid data'));

  if (typeof this.fd !== 'number') {
    return this.once('open', function() {
      this._write(data, encoding, cb);
    });
  }

  var self = this;
  fs.write(this.fd, data, 0, data.length, this.pos, function(er, bytes) {
    if (er) {
      if (self.autoClose) {
        self.destroy();
      }
      return cb(er);
    }
    self.bytesWritten += bytes;
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
WriteStream.prototype.close = ReadStream.prototype.close;

// There is no shutdown() for files.
WriteStream.prototype.destroySoon = WriteStream.prototype.end;

// SyncWriteStream is internal. DO NOT USE.
// This undocumented API was never intended to be made public.
var SyncWriteStream = _SyncWriteStream;
Object.defineProperty(fs, 'SyncWriteStream', {
  configurable: true,
  get: internalUtil.deprecate(() => SyncWriteStream,
                              'fs.SyncWriteStream is deprecated.', 'DEP0061'),
  set: internalUtil.deprecate((val) => { SyncWriteStream = val; },
                              'fs.SyncWriteStream is deprecated.', 'DEP0061')
});

Object.defineProperty(fs, 'async', {
  enumerable: true,
  configurable: true,
  get() {
    return require('internal/async/fs');
  }
});
