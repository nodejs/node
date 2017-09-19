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

const binding = process.binding('fs');
const FSReqWrap = binding.FSReqWrap;

const constants = process.binding('constants').fs;

const {
  S_IFBLK,
  S_IFCHR,
  S_IFDIR,
  S_IFIFO,
  S_IFLNK,
  S_IFMT,
  S_IFREG,
  S_IFSOCK,
  O_APPEND,
  O_CREAT,
  O_RDONLY,
  O_RDWR,
  O_TRUNC,
  O_WRONLY,
  O_SYMLINK,
  F_OK,
  R_OK,
  W_OK,
  X_OK,
  UV_FS_COPYFILE_EXCL
} = constants;

const util = require('util');
const pathModule = require('path');
const { isUint8Array, createPromise, promiseResolve } = process.binding('util');
const fs = exports;
const { Buffer } = require('buffer');
const errors = require('internal/errors');
const { Readable, Writable } = require('stream').Stream;
const EventEmitter = require('events');
const { FSEvent } = process.binding('fs_event_wrap');

const {
  realpathCacheKey,
  stringToFlags,
  SyncWriteStream: InternalSyncWriteStream
} = require('internal/fs');

const { getPathFromURL } = require('internal/url');
const internalUtil = require('internal/util');

Object.defineProperty(exports, 'constants', {
  configurable: false,
  enumerable: true,
  value: constants
});

const kMinPoolSpace = 128;
const kMaxLength = require('buffer').kMaxLength;

const isWindows = process.platform === 'win32';

const DEBUG = process.env.NODE_DEBUG && /fs/.test(process.env.NODE_DEBUG);
const errnoException = util._errnoException;

const writeFlags = O_TRUNC | O_CREAT | O_WRONLY;
const appendFlags = O_APPEND | O_CREAT | O_WRONLY;

const kReadFileBufferLength = 8 * 1024;

const WriteStreamCbSym = Symbol('WriteStreamCallback');
const ReqWrapSym = Symbol('ReqWrapInstance');
const ReqWrapEvSym = Symbol('ReqWrapInstanceEv');
const StreamPoolSym = Symbol('StreamPool');
const StreamStartSym = Symbol('StreamStart');

var pool;
var poolUsed = 0;

function assertOptions(options) {
  if (typeof options !== 'object') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'options',
                               ['string', 'object'],
                               options);
  }
}

function assertEncoding(encoding) {
  if (encoding && !Buffer.isEncoding(encoding) && encoding !== 'buffer') {
    throw new errors.TypeError('ERR_INVALID_OPT_VALUE_ENCODING', encoding);
  }
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
    const backtrace = new Error();
    return function(err) {
      if (err) {
        backtrace.stack = `${err.name}: ${err.message}` +
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
function makeCallbackNoCheck(cb) {
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

function isFd(path) {
  return typeof path === 'number' && path | 0 === path;
}

// Constructor for file stats.
function Stats(
  dev,
  mode,
  nlink,
  uid,
  gid,
  rdev,
  blksize,
  ino,
  size,
  blocks,
  atim_msec,
  mtim_msec,
  ctim_msec,
  birthtim_msec
) {
  this.dev = dev;
  this.mode = mode;
  this.nlink = nlink;
  this.uid = uid;
  this.gid = gid;
  this.rdev = rdev;
  this.blksize = blksize;
  this.ino = ino;
  this.size = size;
  this.blocks = blocks;
  this.atimeMs = atim_msec;
  this.mtimeMs = mtim_msec;
  this.ctimeMs = ctim_msec;
  this.birthtimeMs = birthtim_msec;
  this.atime = new Date(atim_msec + 0.5);
  this.mtime = new Date(mtim_msec + 0.5);
  this.ctime = new Date(ctim_msec + 0.5);
  this.birthtime = new Date(birthtim_msec + 0.5);
}
fs.Stats = Stats;

Stats.prototype._checkModeProperty = function(property) {
  return ((this.mode & S_IFMT) === property);
};

Stats.prototype.isDirectory = function() {
  return this._checkModeProperty(S_IFDIR);
};

Stats.prototype.isFile = function() {
  return this._checkModeProperty(S_IFREG);
};

Stats.prototype.isBlockDevice = function() {
  return this._checkModeProperty(S_IFBLK);
};

Stats.prototype.isCharacterDevice = function() {
  return this._checkModeProperty(S_IFCHR);
};

Stats.prototype.isSymbolicLink = function() {
  return this._checkModeProperty(S_IFLNK);
};

Stats.prototype.isFIFO = function() {
  return this._checkModeProperty(S_IFIFO);
};

Stats.prototype.isSocket = function() {
  return this._checkModeProperty(S_IFSOCK);
};

const statValues = binding.getStatValues();

function statsFromValues() {
  return new Stats(statValues[0], statValues[1], statValues[2], statValues[3],
                   statValues[4], statValues[5],
                   statValues[6] < 0 ? undefined : statValues[6], statValues[7],
                   statValues[8], statValues[9] < 0 ? undefined : statValues[9],
                   statValues[10], statValues[11], statValues[12],
                   statValues[13]);
}

// Don't allow mode to accidentally be overwritten.
Object.defineProperties(fs, {
  F_OK: { enumerable: true, value: F_OK | 0 },
  R_OK: { enumerable: true, value: R_OK | 0 },
  W_OK: { enumerable: true, value: W_OK | 0 },
  X_OK: { enumerable: true, value: X_OK | 0 },
});

function handleError(path, callback, err) {
  err = err || new errors.Error('ERR_INVALID_ARG_TYPE',
                                'path', 'string without null bytes', path);
  if (typeof callback !== 'function')
    throw err;
  process.nextTick(callback, err);
}

function pathCheck(path, cb) {
  if (typeof path !== 'string') {
    if (isUint8Array(path)) {
      if (path.indexOf(0) !== -1)
        return handleError(path, cb);
      return path;
    }
    const normPath = getPathFromURL(path);
    if (typeof normPath !== 'string') {
      if (normPath instanceof Error) {
        return handleError(path, cb, normPath);
      }
      // TODO: this should likely throw. Only strings and buffers should
      // be accepted
      if (path === undefined)
        return null;
      return normPath;
    }
    path = normPath;
  }

  if (path.indexOf('\u0000') !== -1)
    return handleError(path, cb);
  return path;
}

fs.access = function(path, mode, callback) {
  if (typeof mode === 'function') {
    callback = mode;
    mode = F_OK;
  } else if (typeof callback !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }

  path = pathCheck(path, callback);
  if (path === undefined)
    return;

  const req = new FSReqWrap();
  req.oncomplete = makeCallbackNoCheck(callback);
  binding.access(pathModule._makeLong(path), mode | 0, req);
};

fs.accessSync = function(path, mode) {
  if (mode === undefined)
    mode = F_OK;
  else
    mode = mode | 0;

  binding.access(pathModule._makeLong(pathCheck(path)), mode);
};

fs.exists = function(path, callback) {
  path = pathCheck(path, cb);
  if (path === undefined)
    return;
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
  try {
    binding.stat(pathModule._makeLong(pathCheck(path)));
    return true;
  } catch (e) {
    return false;
  }
};

fs.readFile = function(path, options, callback) {
  var encoding, flag;
  if (typeof options === 'function') {
    callback = options;
    flag = O_RDONLY;
  } else {
    if (typeof options === 'string' || options === null ||
        options === undefined) {
      encoding = options;
      flag = O_RDONLY;
    } else {
      assertOptions(options);
      encoding = options.encoding;
      flag = options.flag ? stringToFlags(options.flag) : O_RDONLY;
    }
    assertEncoding(encoding);
    callback = maybeCallback(callback);
  }

  path = pathCheck(path, callback);
  if (path === undefined)
    return;

  const context = new ReadFileContext(callback, encoding, isFd(path));
  if (context.isUserFd) {
    process.nextTick(() => {
      context.req.oncomplete(null, path);
    });
  } else {
    binding.open(pathModule._makeLong(path), flag, 0o666, context.req);
  }
};

function ReadFileContext(callback, encoding, isUserFd) {
  this.req = new FSReqWrap();
  this.req.oncomplete = readFileAfterOpen;
  this.req.context = this;
  this.fd = undefined;
  this.isUserFd = isUserFd;
  this.size = 0;
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

  this.req.oncomplete = readFileAfterRead;
  binding.read(this.fd, buffer, offset, length, -1, this.req);
};

ReadFileContext.prototype.close = function(err) {
  this.req.oncomplete = readFileAfterClose;
  this.err = err;

  if (this.isUserFd) {
    process.nextTick(() => {
      this.req.oncomplete(null);
    });
  } else {
    binding.close(this.fd, this.req);
  }
};

function readFileAfterOpen(err, fd) {
  const context = this.context;

  if (err) {
    context.callback(err);
    return;
  }

  context.fd = fd;
  context.req.oncomplete = readFileAfterStat;
  binding.fstat(fd, context.req);
}

function readFileAfterStat(err) {
  const context = this.context;

  if (err)
    return context.close(err);

  // Use stats array directly to avoid creating an fs.Stats instance just for
  // our internal use.
  // statValues[1] === mode
  // statValues[8] === size
  const size = (statValues[1] & S_IFMT) === S_IFREG ? statValues[8] : 0;
  context.size = size;

  if (size === 0) {
    context.buffers = [];
    context.read();
    return;
  } else if (size > kMaxLength) {
    err = new RangeError('File size is greater than possible Buffer: ' +
                         `0x${kMaxLength.toString(16)} bytes`);
    return context.close(err);
  }

  context.buffer = Buffer.allocUnsafeSlow(size);
  context.read();
}

function readFileAfterRead(err, bytesRead) {
  const context = this.context;

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
    // Unknown size, just read until we don't get bytes.
    context.buffers.push(context.buffer.slice(0, bytesRead));
    context.read();
  }
}

function readFileAfterClose(err) {
  const context = this.context;
  const callback = context.callback;
  var buffer;

  if (context.err || err)
    return callback(context.err || err);

  if (context.size === 0)
    buffer = Buffer.concat(context.buffers, context.pos);
  else if (context.pos < context.size)
    buffer = context.buffer.slice(0, context.pos);
  else
    buffer = context.buffer;

  if (context.encoding) {
    try {
      return callback(null, buffer.toString(context.encoding));
    } catch (err) {
      return callback(err);
    }
  }

  callback(null, buffer);
}

function singleRead(size, fd) {
  var pos = 0;
  const buffer = Buffer.allocUnsafe(size);
  do {
    var bytesRead = binding.read(fd, buffer, pos, size - pos);
    pos += bytesRead;
  } while (bytesRead !== 0 && pos < size);
  if (pos < size) {
    return buffer.slice(0, pos);
  }
  return buffer;
}

function multipleReads(fd) {
  const buffers = [];
  var pos = 0;
  do {
    // The kernel lies about many files.
    // Go ahead and try to read some bytes.
    const newBuffer = Buffer.allocUnsafe(8192);
    var bytesRead = binding.read(fd, newBuffer, 0, 8192);
    if (bytesRead !== 0) {
      buffers.push(newBuffer.slice(0, bytesRead));
    }
    pos += bytesRead;
  } while (bytesRead !== 0);
  // Data was collected into the buffers list.
  return Buffer.concat(buffers, pos);
}

fs.readFileSync = function(path, options) {
  var encoding, flag;
  if (options === undefined || typeof options === 'string' ||
      options === null) {
    encoding = options;
    flag = O_RDONLY;
  } else {
    assertOptions(options);
    encoding = options.encoding;
    flag = options.flag || O_RDONLY;
  }

  assertEncoding(encoding);

  const isUserFd = isFd(path); // file descriptor ownership
  const fd = isUserFd ?
    path : binding.open(pathModule._makeLong(pathCheck(path)), flag, 0o666);

  try {
    binding.fstat(fd);
    // Use stats array directly to avoid creating an fs.Stats instance just for
    // our internal use.
    // statValues[1] === mode
    // statValues[8] === size
    const size = (statValues[1] & S_IFMT) === S_IFREG ? statValues[8] : 0;
    var buffer = size === 0 ? multipleReads(fd) : singleRead(size, fd);
  } finally {
    if (!isUserFd) binding.close(fd);
  }

  if (encoding) return buffer.toString(encoding);
  return buffer;
};

fs.close = function(fd, callback) {
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.close(fd, req);
};

fs.closeSync = function(fd) {
  return binding.close(fd);
};

function modeNum(m, def) {
  if (typeof m === 'number')
    return m;
  if (typeof m === 'string')
    return parseInt(m, 8);
  if (def)
    return def;
  return undefined;
}

fs.open = function(path, flags, mode, callback_) {
  const callback = makeCallback(arguments[arguments.length - 1]);

  path = pathCheck(path, callback);
  if (path === undefined)
    return;

  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.open(pathModule._makeLong(path),
               stringToFlags(flags),
               modeNum(mode, 0o666),
               req);
};

fs.openSync = function(path, flags, mode) {
  mode = modeNum(mode, 0o666);
  path = pathCheck(path);
  return binding.open(pathModule._makeLong(path), stringToFlags(flags), mode);
};

fs.read = function(fd, buffer, offset, length, position, callback) {
  if (length === 0) {
    return process.nextTick(() => {
      callback && callback(null, 0, buffer);
    });
  }
  const req = new FSReqWrap();
  req.oncomplete = (err, bytesRead) => {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback && callback(err, bytesRead || 0, buffer);
  };
  binding.read(fd, buffer, offset, length, position, req);
};

Object.defineProperty(fs.read, internalUtil.customPromisifyArgs,
                      { value: ['bytesRead', 'buffer'], enumerable: false });

fs.readSync = function(fd, buffer, offset, length, position) {
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
  function wrapper(err, written) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback(err, written || 0, buffer);
  }

  const req = new FSReqWrap();
  req.oncomplete = wrapper;

  if (typeof buffer === 'object' && isUint8Array(buffer)) {
    callback = maybeCallback(arguments[arguments.length - 1]);
    if (typeof offset !== 'number') {
      offset = 0;
    }
    if (typeof length !== 'number') {
      length = buffer.length - offset;
    }
    if (typeof position !== 'number') {
      position = null;
    }
    return binding.writeBuffer(fd, buffer, offset, length, position, req);
  }

  // TODO: this should probably throw instead of being coerced to a string
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
  if (typeof buffer === 'object' && isUint8Array(buffer)) {
    if (position === undefined)
      position = null;
    if (typeof offset !== 'number')
      offset = 0;
    if (typeof length !== 'number')
      length = buffer.length - offset;
    return binding.writeBuffer(fd, buffer, offset, length, position);
  }
  // TODO: this should probably throw instead of being coerced to a string
  if (typeof buffer !== 'string')
    buffer += '';
  if (offset === undefined)
    offset = null;
  return binding.writeString(fd, buffer, offset, length, position);
};

fs.rename = function(oldPath, newPath, callback) {
  callback = makeCallback(callback);
  oldPath = pathCheck(oldPath, callback);
  if (oldPath === undefined)
    return;

  newPath = pathCheck(newPath, callback);
  if (newPath === undefined)
    return;

  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.rename(pathModule._makeLong(oldPath),
                 pathModule._makeLong(newPath),
                 req);
};

fs.renameSync = function(oldPath, newPath) {
  return binding.rename(pathModule._makeLong(pathCheck(oldPath)),
                        pathModule._makeLong(pathCheck(newPath)));
};

fs.truncate = function(path, len, callback) {
  if (typeof path === 'number') {
    return fs.ftruncate(path, len, callback);
  }
  if (typeof len === 'function') {
    callback = len;
    len = 0;
  } else {
    if (len === undefined) {
      len = 0;
    }
    callback = maybeCallback(callback);
  }

  path = pathCheck(path, callback);
  if (path === undefined)
    return;

  const req = new FSReqWrap();
  req.oncomplete = (err1, fd) => {
    if (err1) return callback(err1);
    req.oncomplete = (err2) => {
      req.oncomplete = (err3) => {
        callback(err2 || err3);
      };
      binding.close(fd, req);
    };
    binding.ftruncate(fd, len, req);
  };
  binding.open(pathModule._makeLong(path), O_RDWR, 0o666, req);
};

fs.truncateSync = function(path, len) {
  if (len === undefined) {
    len = 0;
  }
  if (typeof path === 'number') {
    // Legacy
    return binding.ftruncate(path, len);
  }
  // Allow error to be thrown, but still close fd.
  const fd = binding.open(pathModule._makeLong(pathCheck(path)), O_RDWR, 0o666);
  var ret;

  try {
    ret = binding.ftruncate(fd, len);
  } finally {
    binding.close(fd);
  }
  return ret;
};

fs.ftruncate = function(fd, len, callback) {
  if (typeof len === 'function') {
    callback = makeCallbackNoCheck(len);
    len = 0;
  } else {
    if (len === undefined) {
      len = 0;
    }
    callback = makeCallback(callback);
  }
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.ftruncate(fd, len, req);
};

fs.ftruncateSync = function(fd, len) {
  if (len === undefined) {
    len = 0;
  }
  return binding.ftruncate(fd, len);
};

fs.rmdir = function(path, callback) {
  callback = maybeCallback(callback);
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.rmdir(pathModule._makeLong(path), req);
};

fs.rmdirSync = function(path) {
  return binding.rmdir(pathModule._makeLong(pathCheck(path)));
};

fs.fdatasync = function(fd, callback) {
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fdatasync(fd, req);
};

fs.fdatasyncSync = function(fd) {
  return binding.fdatasync(fd);
};

fs.fsync = function(fd, callback) {
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fsync(fd, req);
};

fs.fsyncSync = function(fd) {
  return binding.fsync(fd);
};

fs.mkdir = function(path, mode, callback) {
  if (typeof mode === 'function') {
    callback = makeCallbackNoCheck(mode);
  } else {
    callback = makeCallback(callback);
  }
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.mkdir(pathModule._makeLong(path), modeNum(mode, 0o777), req);
};

fs.mkdirSync = function(path, mode) {
  return binding.mkdir(pathModule._makeLong(pathCheck(path)),
                       modeNum(mode, 0o777));
};

fs.readdir = function(path, options, callback) {
  var encoding;
  if (typeof options === 'function') {
    callback = makeCallbackNoCheck(options);
  } else {
    if (typeof options === 'string' || options === null ||
        options === undefined) {
      encoding = options;
    } else {
      assertOptions(options);
      encoding = options.encoding;
    }
    assertEncoding(encoding);
    callback = makeCallback(callback);
  }
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readdir(pathModule._makeLong(path), encoding, req);
};

fs.readdirSync = function(path, options) {
  var encoding;
  if (options === undefined || typeof options === 'string' ||
      options === null) {
    encoding = options;
  } else {
    assertOptions(options);
    encoding = options.encoding;
  }
  assertEncoding(encoding);
  return binding.readdir(pathModule._makeLong(pathCheck(path)), encoding);
};

fs.fstat = function(fd, callback) {
  const req = new FSReqWrap();
  req.oncomplete = makeStatsCallback(callback);
  binding.fstat(fd, req);
};

fs.lstat = function(path, callback) {
  callback = makeStatsCallback(callback);
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.lstat(pathModule._makeLong(path), req);
};

fs.stat = function(path, callback) {
  callback = makeStatsCallback(callback);
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.stat(pathModule._makeLong(path), req);
};

fs.fstatSync = function(fd) {
  binding.fstat(fd);
  return statsFromValues();
};

fs.lstatSync = function(path) {
  binding.lstat(pathModule._makeLong(pathCheck(path)));
  return statsFromValues();
};

fs.statSync = function(path) {
  binding.stat(pathModule._makeLong(pathCheck(path)));
  return statsFromValues();
};

fs.readlink = function(path, options, callback) {
  var encoding;
  if (typeof options === 'function') {
    callback = makeCallbackNoCheck(options);
  } else {
    if (typeof options === 'string' || options === null ||
        options === undefined) {
      encoding = options;
    } else {
      assertOptions(options);
      encoding = options.encoding;
    }
    assertEncoding(encoding);
    callback = makeCallback(callback);
  }
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readlink(pathModule._makeLong(path), encoding, req);
};

fs.readlinkSync = function(path, options) {
  var encoding;
  if (options === undefined || typeof options === 'string' ||
      options === null) {
    encoding = options;
  } else {
    assertOptions(options);
    encoding = options.encoding;
  }
  assertEncoding(encoding);
  return binding.readlink(pathModule._makeLong(pathCheck(path)), encoding);
};

function preprocessSymlinkDestination(path, type, linkPath) {
  if (!isWindows) {
    // No preprocessing is needed on Unix.
    return path;
  }
  if (type === 'junction') {
    // Junctions paths need to be absolute and \\?\-prefixed.
    // A relative target is relative to the link's parent directory.
    return pathModule._makeLong(pathModule.resolve(linkPath, '..', path));
  }
  // Windows symlinks don't tolerate forward slashes.
  return path.replace(/\//g, '\\');
}

fs.symlink = function(target, path, type_, callback_) {
  const type = typeof type_ === 'string' ? type_ : null;
  const callback = makeCallback(arguments[arguments.length - 1]);

  target = pathCheck(target, callback);
  if (target === undefined)
    return;
  path = pathCheck(path, callback);
  if (path === undefined)
    return;

  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.symlink(preprocessSymlinkDestination(target, type, path),
                  pathModule._makeLong(path),
                  type,
                  req);
};

fs.symlinkSync = function(target, path, type) {
  type = typeof type === 'string' ? type : null;
  target = pathCheck(target);
  path = pathCheck(path);

  return binding.symlink(preprocessSymlinkDestination(target, type, path),
                         pathModule._makeLong(path),
                         type);
};

fs.link = function(existingPath, newPath, callback) {
  callback = makeCallback(callback);

  existingPath = pathCheck(existingPath, callback);
  if (existingPath === undefined)
    return;
  newPath = pathCheck(newPath, callback);
  if (newPath === undefined)
    return;

  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.link(pathModule._makeLong(existingPath),
               pathModule._makeLong(newPath),
               req);
};

fs.linkSync = function(existingPath, newPath) {
  return binding.link(pathModule._makeLong(pathCheck(existingPath)),
                      pathModule._makeLong(pathCheck(newPath)));
};

fs.unlink = function(path, callback) {
  callback = makeCallback(callback);
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.unlink(pathModule._makeLong(path), req);
};

fs.unlinkSync = function(path) {
  return binding.unlink(pathModule._makeLong(pathCheck(path)));
};

fs.fchmod = function(fd, mode, callback) {
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fchmod(fd, modeNum(mode), req);
};

fs.fchmodSync = function(fd, mode) {
  return binding.fchmod(fd, modeNum(mode));
};

if (O_SYMLINK !== undefined) {
  const chownFlag = O_WRONLY | O_SYMLINK;
  fs.lchmod = function(path, mode, callback) {
    callback = maybeCallback(callback);
    path = pathCheck(path, callback);
    if (path === undefined)
      return;

    const req = new FSReqWrap();
    req.oncomplete = (err, fd) => {
      if (err) return callback(err);
      // Prefer to return the chmod error, if one occurs,
      // but still try to close, and report closing errors if they occur.
      req.oncomplete = (err) => {
        req.oncomplete = (err2) => {
          callback(err || err2);
        };
        binding.close(fd, req);
      };
      binding.fchmod(fd, modeNum(mode), req);
    };

    binding.open(pathModule._makeLong(path), chownFlag, 0o666, req);
  };

  fs.lchmodSync = function(path, mode) {
    const flag = chownFlag;
    const fd = binding.open(pathModule._makeLong(pathCheck(path)), flag, 0o666);

    // Prefer to return the chmod error, if one occurs,
    // but still try to close, and report closing errors if they occur.
    var ret;
    try {
      ret = binding.fchmod(fd, modeNum(mode));
    } catch (err) {
      try {
        binding.close(fd);
      } catch (ignore) {}
      throw err;
    }
    binding.close(fd);
    return ret;
  };

  fs.lchown = function(path, uid, gid, callback) {
    callback = maybeCallback(callback);
    path = pathCheck(path, callback);
    if (path === undefined)
      return;

    const req = new FSReqWrap();
    req.oncomplete = (err, fd) => {
      if (err) return callback(err);
      req.oncomplete = (err) => { callback(err); };
      binding.fchown(fd, uid, gid, req);
    };

    binding.open(pathModule._makeLong(path), chownFlag, 0o666, req);
  };

  fs.lchownSync = function(path, uid, gid) {
    const fd = binding.open(
      pathModule._makeLong(pathCheck(path)), chownFlag, 0o666);
    return binding.fchown(fd, uid, gid);
  };
}

fs.chmod = function(path, mode, callback) {
  callback = makeCallback(callback);
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.chmod(pathModule._makeLong(path), modeNum(mode), req);
};

fs.chmodSync = function(path, mode) {
  path = pathCheck(path);
  return binding.chmod(pathModule._makeLong(path), modeNum(mode));
};

fs.fchown = function(fd, uid, gid, callback) {
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fchown(fd, uid, gid, req);
};

fs.fchownSync = function(fd, uid, gid) {
  return binding.fchown(fd, uid, gid);
};

fs.chown = function(path, uid, gid, callback) {
  callback = makeCallback(callback);
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.chown(pathModule._makeLong(path), uid, gid, req);
};

fs.chownSync = function(path, uid, gid) {
  return binding.chown(pathModule._makeLong(pathCheck(path)), uid, gid);
};

// Converts Date or number to a fractional UNIX timestamp
function toUnixTimestamp(time) {
  if (typeof time === 'string') {
    const timeNumber = +time;
    // eslint-disable-next-line eqeqeq
    if (timeNumber == time)
      return timeNumber;
  } else if (Number.isFinite(time)) {
    if (time < 0) {
      return Date.now() / 1000;
    }
    return time;
  }
  if (util.isDate(time)) {
    // Convert to 123.456 UNIX timestamp
    return time.getTime() / 1000;
  }
  throw new errors.Error('ERR_INVALID_ARG_TYPE',
                         'time',
                         ['Date', 'time in seconds'],
                         time);
}

// Exported for unit tests, not for public consumption
fs._toUnixTimestamp = toUnixTimestamp;

fs.utimes = function(path, atime, mtime, callback) {
  callback = makeCallback(callback);
  path = pathCheck(path, callback);
  if (path === undefined)
    return;
  const req = new FSReqWrap();
  req.oncomplete = callback;
  binding.utimes(pathModule._makeLong(path),
                 toUnixTimestamp(atime),
                 toUnixTimestamp(mtime),
                 req);
};

fs.utimesSync = function(path, atime, mtime) {
  binding.utimes(pathModule._makeLong(pathCheck(path)),
                 toUnixTimestamp(atime),
                 toUnixTimestamp(mtime));
};

fs.futimes = function(fd, atime, mtime, callback) {
  const req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.futimes(fd, toUnixTimestamp(atime), toUnixTimestamp(mtime), req);
};

fs.futimesSync = function(fd, atime, mtime) {
  binding.futimes(fd, toUnixTimestamp(atime), toUnixTimestamp(mtime));
};

function writeAll(fd, isUserFd, data, encoding, flag, callback) {
  const buffer = typeof data === 'object' && isUint8Array(data) ?
    data : Buffer.from('' + data, encoding || 'utf8');
  var position = flag & O_APPEND ? null : 0;
  var offset = 0;
  var length = buffer.length;
  const req = new FSReqWrap();
  req.oncomplete = (err, written = 0) => {
    if (err) {
      if (isUserFd) {
        callback(err);
      } else {
        req.oncomplete = () => { callback(err); };
        binding.close(fd, req);
      }
    } else if (written === length) {
      if (isUserFd) {
        callback(null);
      } else {
        req.oncomplete = callback;
        binding.close(fd, req);
      }
    } else {
      offset += written;
      length -= written;
      if (position !== null) {
        position += written;
      }
      binding.writeBuffer(fd, buffer, offset, length, position, req);
    }
  };
  binding.writeBuffer(fd, buffer, offset, length, position, req);
}

fs.writeFile = function(path, data, options, callback) {
  _writeFile(path, data, options, callback, writeFlags, false);
};

fs.appendFile = function(path, data, options, callback) {
  _writeFile(path, data, options, callback, appendFlags, true);
};

function _writeFile(path, data, options, callback, defaultFlag, checkFD) {
  var encoding, mode, flag;
  if (typeof options === 'function') {
    callback = options;
    mode = 0o666;
    flag = defaultFlag;
    encoding = 'utf8';
  } else {
    if (typeof options === 'string' || options === null ||
        options === undefined) {
      encoding = options;
      mode = 0o666;
      flag = defaultFlag;
    } else {
      assertOptions(options);
      encoding = options.encoding;
      mode = modeNum(options.mode, 0o666);
      flag = (!options.flag || checkFD && isFd(path)) ?
        defaultFlag : stringToFlags(options.flag);
    }
    assertEncoding(encoding);
    callback = maybeCallback(callback);
  }

  if (isFd(path)) {
    writeAll(path, true, data, encoding, flag, callback);
    return;
  }

  path = pathCheck(path, callback);
  if (path === undefined)
    return;

  const req = new FSReqWrap();
  req.oncomplete = (err, fd) => {
    if (err) {
      callback(err);
    } else {
      writeAll(fd, false, data, encoding, flag, callback);
    }
  };

  binding.open(pathModule._makeLong(path), flag, mode, req);
}

fs.writeFileSync = function(path, data, options) {
  _writeFileSync(path, data, options, writeFlags, false);
};

fs.appendFileSync = function(path, data, options) {
  _writeFileSync(path, data, options, appendFlags, true);
};

function _writeFileSync(path, data, options, defaultFlag, checkFD) {
  var encoding, mode, flag;
  if (options === undefined || typeof options === 'string' ||
      options === null) {
    encoding = options;
    mode = 0o666;
    flag = defaultFlag;
  } else {
    assertOptions(options);
    encoding = options.encoding;
    mode = modeNum(options.mode, 0o666);
    // force append behavior when using a supplied file descriptor
    flag = (!options.flag || checkFD && isFd(path)) ?
      defaultFlag : stringToFlags(options.flag);
  }

  if (typeof data !== 'object' || !isUint8Array(data)) {
    assertEncoding(encoding);
    data = Buffer.from('' + data, encoding || 'utf8');
  }

  const isUserFd = isFd(path); // file descriptor ownership
  const fd = isUserFd ?
    path : binding.open(pathModule._makeLong(pathCheck(path)), flag, mode);

  var offset = 0;
  var length = data.length;
  var position = flag & O_APPEND ? null : 0;
  try {
    while (length > 0) {
      const written = binding.writeBuffer(fd, data, offset, length, position);
      offset += written;
      length -= written;
      if (position !== null) {
        position += written;
      }
    }
  } finally {
    if (!isUserFd) binding.close(fd);
  }
}

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
  filename = pathCheck(filename);
  const err = this._handle.start(pathModule._makeLong(filename),
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
  filename = pathCheck(filename);

  var persistent = true;
  var recursive = false;
  var encoding;
  if (options === undefined || typeof options === 'string' ||
      options === null) {
    encoding = options;
  } else if (typeof options === 'function') {
    listener = options;
    encoding = 'utf8';
  } else {
    assertOptions(options);
    encoding = options.encoding;
    persistent = options.persistent === undefined ? true : options.persistent;
    recursive = options.recursive === undefined ? false : options.recursive;
  }
  assertEncoding(encoding);

  const watcher = new FSWatcher();
  watcher.start(filename, persistent, recursive, encoding);

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

  this._handle = new binding.StatWatcher();

  // uv_fs_poll is a little more powerful than ev_stat but we curb it for
  // the sake of backwards compatibility
  var oldStatus = -1;

  this._handle.onchange = (newStatus) => {
    if (oldStatus === -1 &&
        newStatus === -1 &&
        statValues[2/*new nlink*/] === statValues[16/*old nlink*/]) return;

    oldStatus = newStatus;
    this.emit('change', statsFromValues(), statsFromPrevValues());
  };

  this._handle.onstop = () => {
    process.nextTick(emitStop, this);
  };
}
util.inherits(StatWatcher, EventEmitter);

StatWatcher.prototype.start = function(filename, persistent, interval) {
  filename = pathCheck(filename);
  this._handle.start(pathModule._makeLong(filename), persistent, interval);
};

StatWatcher.prototype.stop = function() {
  this._handle.stop();
};

const statWatchers = new Map();

fs.watchFile = function(filename, options, listener) {
  filename = pathModule.resolve(pathCheck(filename));
  var interval = 5007;
  var persistent = true;

  // Poll interval in milliseconds. 5007 is what libev used to use. It's
  // a little on the slow side but let's stick with it for now to keep
  // behavioral changes to a minimum.
  if (typeof options === 'object' && options !== null) {
    interval = options.interval === undefined ? 5007 : options.interval;
    persistent = options.persistent === undefined ? 5007 : options.persistent;
  } else {
    listener = options;
  }
  if (typeof listener !== 'function') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'listener',
                               'function',
                               listener);
  }

  var stat = statWatchers.get(filename);

  if (stat === undefined) {
    stat = new StatWatcher();
    stat.start(filename, persistent, interval);
    statWatchers.set(filename, stat);
  }

  stat.addListener('change', listener);
  return stat;
};

fs.unwatchFile = function(filename, listener) {
  filename = pathModule.resolve(pathCheck(filename));
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
      if (str.charCodeAt(i) !== 47) // '/'
        return str.slice(0, i);
    }
    return str;
  };
}

function encodeRealpathResult(result, encoding) {
  if (!encoding || encoding === 'utf8')
    return result;
  const asBuffer = Buffer.from(result);
  if (encoding === 'buffer') {
    return asBuffer;
  }
  return asBuffer.toString(encoding);
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

fs.realpathSync = function realpathSync(p, options) {
  var encoding, cache;

  p = pathModule.resolve(`${pathCheck(p)}`);

  if (options === undefined || typeof options === 'string' ||
      options === null) {
    encoding = options;
  } else {
    assertOptions(options);
    cache = options[realpathCacheKey];
    const maybeCachedResult = cache !== undefined && cache.get(p);
    if (maybeCachedResult) {
      return maybeCachedResult;
    }
    encoding = options.encoding;
  }
  assertEncoding(encoding);

  var seenLinks;
  const knownHard = Object.create(null);
  const original = p;

  // The partial path without a trailing slash (except when pointing at a root)
  var base;
  // The partial path scanned in the previous round, with slash
  var previous;

  // The partial path so far, including a trailing slash if any
  // Skip over roots
  var current = base = splitRoot(p);
  // current character position in p
  var pos = current.length;

  // On windows, check that the root exists. On unix there is no need.
  if (isWindows) {
    binding.lstat(pathModule._makeLong(base));
    knownHard[base] = true;
  } else {
    seenLinks = Object.create(null);
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
    if (knownHard[base] || (cache !== undefined && cache.get(base) === base)) {
      if ((statValues[1/*mode*/] & S_IFMT) === S_IFIFO ||
          (statValues[1/*mode*/] & S_IFMT) === S_IFSOCK) {
        break;
      }
      continue;
    }

    var resolvedLink;
    const maybeCachedResolved = cache !== undefined && cache.get(base);
    if (maybeCachedResolved) {
      resolvedLink = maybeCachedResolved;
    } else {
      // Use stats array directly to avoid creating an fs.Stats instance just
      // for our internal use.

      const baseLong = pathModule._makeLong(base);
      binding.lstat(baseLong);

      if ((statValues[1/*mode*/] & S_IFMT) !== S_IFLNK) {
        knownHard[base] = true;
        if (cache !== undefined) cache.set(base, base);
        continue;
      }

      // Read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      var linkTarget = null;
      var id;
      if (!isWindows) {
        const dev = statValues[0/*dev*/].toString(32);
        const ino = statValues[7/*ino*/].toString(32);
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

      if (cache !== undefined) cache.set(base, resolvedLink);
      if (!isWindows) seenLinks[id] = linkTarget;
    }

    // Resolve the link, then start over
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

  if (cache !== undefined) cache.set(original, p);
  return encodeRealpathResult(p, encoding);
};

fs.realpath = function realpath(p, options, callback) {
  var encoding;
  if (typeof options === 'function') {
    callback = options;
  } else {
    if (typeof options === 'string' || options === null ||
      options === undefined) {
      encoding = options;
    } else {
      assertOptions(options);
      encoding = options.encoding;
    }
    assertEncoding(encoding);
    callback = maybeCallback(callback);
  }

  p = pathCheck(p, callback);
  if (p === undefined)
    return;
  var path = pathModule.resolve(`${p}`);

  var seenLinks;
  const knownHard = Object.create(null);

  // The partial path without a trailing slash (except when pointing at a root)
  var base;
  // The partial path scanned in the previous round, with slash
  var previous;

  // The partial path so far, including a trailing slash if any
  var current = base = splitRoot(path);
  // Current character position in p
  var pos = current.length;

  const req = new FSReqWrap();

  // On windows, check that the root exists. On unix there is no need.
  if (isWindows) {
    req.oncomplete = (err) => {
      if (err) return callback(err);
      knownHard[base] = true;
      LOOP();
    };
    binding.lstat(pathModule._makeLong(base), req);
  } else {
    seenLinks = Object.create(null);
    process.nextTick(LOOP);
  }

  // Walk down the path, swapping out linked path parts for their real
  // values
  function LOOP() {
    // Stop if scanned past end of path
    if (pos >= path.length) {
      return callback(null, encodeRealpathResult(path, encoding));
    }

    // Find the next part
    const result = nextPart(path, pos);
    previous = current;
    if (result === -1) {
      const last = path.slice(pos);
      current += last;
      base = previous + last;
      pos = path.length;
    } else {
      current += path.slice(pos, result + 1);
      base = previous + path.slice(pos, result);
      pos = result + 1;
    }

    // Continue if not a symlink, break if a pipe/socket
    if (knownHard[base]) {
      if ((statValues[1/*mode*/] & S_IFMT) === S_IFIFO ||
          (statValues[1/*mode*/] & S_IFMT) === S_IFSOCK) {
        return callback(null, encodeRealpathResult(path, encoding));
      }
      return process.nextTick(LOOP);
    }

    req.oncomplete = gotStat;
    binding.lstat(pathModule._makeLong(base), req);
  }

  function gotStat(err) {
    if (err) return callback(err);

    // Use stats array directly to avoid creating an fs.Stats instance just for
    // our internal use.

    // If not a symlink, skip to the next path part
    if ((statValues[1/*mode*/] & S_IFMT) !== S_IFLNK) {
      knownHard[base] = true;
      return process.nextTick(LOOP);
    }

    // stat & read the link if not read before
    // call gotTarget as soon as the link target is known
    // dev/ino always return 0 on windows, so skip the check.
    let id;
    if (!isWindows) {
      const dev = statValues[0/*ino*/].toString(32);
      const ino = statValues[7/*ino*/].toString(32);
      id = `${dev}:${ino}`;
      if (seenLinks[id]) {
        return gotTarget(null, seenLinks[id], base);
      }
    }
    req.oncomplete = (err) => {
      if (err) return callback(err);
      req.oncomplete = (err, target) => {
        if (!isWindows) seenLinks[id] = target;
        gotTarget(err, target);
      };
      binding.readlink(pathModule._makeLong(base), encoding, req);
    };
    binding.stat(pathModule._makeLong(base), req);
  }

  function gotTarget(err, target, base) {
    if (err) return callback(err);

    const resolvedLink = pathModule.resolve(previous, target);
    // Resolve the link, then start over
    path = pathModule.resolve(resolvedLink, path.slice(pos));
    current = base = splitRoot(path);
    pos = current.length;

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      req.oncomplete = (err) => {
        if (err) return callback(err);
        knownHard[base] = true;
        LOOP();
      };
      binding.lstat(pathModule._makeLong(base), req);
    } else {
      process.nextTick(LOOP);
    }
  }
};

fs.mkdtemp = function(prefix, options, callback) {
  if (typeof prefix !== 'string' || prefix === '') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'prefix',
                               'string',
                               prefix);
  }
  var encoding;
  if (typeof options === 'function') {
    callback = makeCallbackNoCheck(options);
  } else {
    if (typeof options === 'string' || options === null ||
      options === undefined) {
      encoding = options;
    } else {
      assertOptions(options);
      encoding = options.encoding;
    }

    assertEncoding(encoding);
    callback = makeCallback(callback);
  }

  var offset = 0;
  while (offset++ < prefix.length) {
    if (prefix.charCodeAt(offset) === 0) {
      process.nextTick(callback, new errors.Error('ERR_INVALID_ARG_TYPE',
                                                  'prefix',
                                                  'string without null bytes',
                                                  prefix));
      return;
    }
  }

  const req = new FSReqWrap();
  req.oncomplete = callback;

  binding.mkdtemp(prefix + 'XXXXXX', encoding, req);
};

fs.mkdtempSync = function(prefix, options) {
  if (typeof prefix !== 'string' || prefix === '') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'prefix',
                               'string',
                               prefix);
  }
  var encoding;
  if (options === undefined || typeof options === 'string' ||
      options === null) {
    encoding = options;
  } else {
    assertOptions(options);
    encoding = options.encoding;
  }

  assertEncoding(encoding);
  var offset = 0;
  while (offset++ < prefix.length) {
    if (prefix.charCodeAt(offset) === 0) {
      throw new errors.Error('ERR_INVALID_ARG_TYPE',
                             'prefix',
                             'string without null bytes',
                             prefix);
    }
  }
  return binding.mkdtemp(prefix + 'XXXXXX', encoding);
};

// Define copyFile() flags.
Object.defineProperties(fs.constants, {
  COPYFILE_EXCL: { enumerable: true, value: UV_FS_COPYFILE_EXCL }
});

fs.copyFile = function(src, dest, flags, callback) {
  if (typeof flags === 'function') {
    callback = flags;
    flags = 0;
  } else if (typeof callback !== 'function') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'callback', 'function');
  }

  src = pathCheck(src, callback);
  if (src === undefined)
    return;
  dest = pathCheck(dest, callback);
  if (dest === undefined)
    return;

  const req = new FSReqWrap();
  req.oncomplete = makeCallbackNoCheck(callback);
  binding.copyFile(pathModule._makeLong(src),
                   pathModule._makeLong(dest),
                   flags | 0,
                   req);
};

fs.copyFileSync = function(src, dest, flags) {
  binding.copyFile(pathModule._makeLong(pathCheck(src)),
                   pathModule._makeLong(pathCheck(dest)),
                   flags | 0);
};

fs.createReadStream = function(path, options) {
  return new ReadStream(path, options, StreamStartSym);
};

util.inherits(ReadStream, Readable);
fs.ReadStream = ReadStream;

function ReadStream(path, options, instanceSymbol) {
  if (instanceSymbol !== StreamStartSym && !(this instanceof ReadStream))
    return new ReadStream(path, options, StreamStartSym);

  this.bytesRead = 0;
  this.start = undefined;
  this.end = undefined;
  this.pos = undefined;
  if (options === undefined || typeof options === 'string' ||
      options === null) {
    options = { encoding: options };
    this.fd = null;
    this.flags = O_RDONLY;
    this.mode = 0o666;
    this.autoClose = true;
  } else {
    assertOptions(options);
    this.fd = options.fd === undefined ? null : options.fd;
    this.flags = options.flags === undefined ?
      O_RDONLY : stringToFlags(options.flags);
    this.mode = options.mode === undefined ?
      0o666 : modeNum(options.mode, 0o666);
    this.autoClose = options.autoClose === undefined ? true : options.autoClose;

    if (options.start !== undefined) {
      if (typeof options.start !== 'number') {
        throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                   'start',
                                   'number',
                                   options.start);
      }
      if (options.end === undefined) {
        this.end = Infinity;
      } else {
        if (typeof options.end !== 'number') {
          throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                     'end',
                                     'number',
                                     options.end);
        }
        this.end = options.end;
      }
      if (options.start > this.end) {
        const errVal = `{start: ${options.start}, end: ${this.end}}`;
        throw new errors.RangeError('ERR_VALUE_OUT_OF_RANGE',
                                    'start',
                                    '<= "end"',
                                    errVal);
      }
      this.pos = this.start = options.start;
    }
  }
  assertEncoding(options.encoding);

  Readable.call(this, options);

  // A little bit higher water marks by default
  if (options.highWaterMark === undefined)
    this._readableState.highWaterMark = 64 * 1024;

  const req = new FSReqWrap();
  req.stream = this;
  req.oncomplete = onRSRead;
  this[ReqWrapSym] = req;
  this[StreamPoolSym] = undefined;
  this[StreamStartSym] = undefined;

  if (typeof this.fd !== 'number') {
    this.path = pathCheck(path);
    this.open();
  } else {
    this.path = path;
  }

  this.on('end', () => {
    if (this.autoClose) {
      this.destroy();
    }
  });
}

fs.FileReadStream = fs.ReadStream; // Support the legacy name

function onRSOpen(er, fd) {
  const stream = this.stream;
  if (er) {
    if (stream.autoClose) {
      stream.destroy();
    }
    stream.emit('error', er);
    return;
  }

  this.oncomplete = onRSRead;
  stream.fd = fd;
  stream.emit('open', fd);
  // Start the flow of data.
  stream.read();
}

ReadStream.prototype.open = function() {
  this[ReqWrapSym].oncomplete = onRSOpen;
  binding.open(
    pathModule._makeLong(this.path), this.flags, this.mode, this[ReqWrapSym]);
};

function onRSRead(er, bytesRead) {
  const stream = this.stream;
  if (er) {
    if (stream.autoClose)
      stream.destroy();
    stream.emit('error', er);
  } else if (bytesRead > 0) {
    stream.bytesRead += bytesRead;
    const start = stream[StreamStartSym];
    stream.push(stream[StreamPoolSym].slice(start, start + bytesRead));
  } else {
    stream.push(null);
  }
}

ReadStream.prototype._read = function(n) {
  if (typeof this.fd !== 'number') {
    return this.once('open', () => {
      this._read(n);
    });
  }

  if (this.destroyed)
    return;

  if (pool === undefined || pool.length - poolUsed < kMinPoolSpace) {
    // Discard the old pool.
    pool = Buffer.allocUnsafe(this._readableState.highWaterMark);
    poolUsed = 0;
  }

  // Grab another reference to the pool in the case that while we're
  // in the thread pool another binding.read() finishes up the pool, and
  // allocates a new one.
  this[StreamPoolSym] = pool;
  this[StreamStartSym] = poolUsed;
  var toRead = Math.min(pool.length - poolUsed, n);

  if (this.pos !== undefined)
    toRead = Math.min(this.end - this.pos + 1, toRead);

  // Already read everything we were supposed to read!
  // treat as EOF.
  if (toRead <= 0)
    return this.push(null);

  // The actual read.
  binding.read(this.fd, pool, poolUsed, toRead, this.pos, this[ReqWrapSym]);

  // Move the pool positions, and internal position for reading.
  if (this.pos !== undefined)
    this.pos += toRead;
  poolUsed += toRead;
};

ReadStream.prototype._destroy = function(err, cb) {
  this.close((err2) => {
    cb(err || err2);
  });
};

function onRSClose(er) {
  if (er)
    this.stream.emit('error', er);
  else
    this.stream.emit('close');
}

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

  this[ReqWrapSym].oncomplete = onRSClose;
  binding.close(this.fd, this[ReqWrapSym]);

  this.fd = null;
};

// Needed because it will be called with arguments
// that do not match the this.close() signature
function closeOnOpen(fd) {
  this.close();
}

fs.createWriteStream = function(path, options) {
  return new WriteStream(path, options, StreamStartSym);
};

util.inherits(WriteStream, Writable);
fs.WriteStream = WriteStream;
function WriteStream(path, options, instanceSymbol) {
  if (instanceSymbol !== StreamStartSym && !(this instanceof WriteStream))
    return new WriteStream(path, options, StreamStartSym);

  this.bytesWritten = 0;
  this.pos = undefined;
  this.start = undefined;
  var encoding;
  if (options === undefined || typeof options === 'string' ||
      options === null) {
    encoding = options;
    options = undefined;
    this.fd = null;
    this.flags = writeFlags;
    this.mode = 0o666;
    this.autoClose = true;
  } else {
    assertOptions(options);
    encoding = options.encoding;
    this.fd = options.fd === undefined ? null : options.fd;
    this.flags = options.flags === undefined ?
      writeFlags : stringToFlags(options.flags);
    this.mode = options.mode === undefined ?
      0o666 : modeNum(options.mode, 0o666);
    this.autoClose = options.autoClose === undefined ? true : options.autoClose;

    if (options.start !== undefined) {
      if (typeof options.start !== 'number') {
        throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                   'start',
                                   'number',
                                   options.start);
      }
      if (options.start < 0) {
        const errVal = `{start: ${options.start}}`;
        throw new errors.RangeError('ERR_VALUE_OUT_OF_RANGE',
                                    'start',
                                    '>= 0',
                                    errVal);
      }
      this.pos = this.start = options.start;
    }
  }
  assertEncoding(encoding);

  Writable.call(this, options);

  const req = new FSReqWrap();
  req.stream = this;
  req.oncomplete = onWSWrite;
  this[ReqWrapSym] = req;
  this[ReqWrapEvSym] = undefined;
  this[WriteStreamCbSym] = undefined;

  if (encoding)
    this.setDefaultEncoding(encoding);

  if (typeof this.fd !== 'number') {
    this.path = pathCheck(path);
    this.open();
  } else {
    this.path = path;
  }

  // Dispose on finish.
  this.once('finish', () => {
    if (this.autoClose) {
      this.close();
    }
  });
}

fs.FileWriteStream = fs.WriteStream; // Support the legacy name

function onWSOpen(er, fd) {
  const stream = this.stream;
  if (er) {
    if (stream.autoClose) {
      stream.destroy();
    }
    stream.emit('error', er);
    return;
  }

  this.oncomplete = onWSWrite;
  stream.fd = fd;
  stream.emit('open', fd);
}

WriteStream.prototype.open = function() {
  this[ReqWrapSym].oncomplete = onWSOpen;
  binding.open(
    pathModule._makeLong(this.path), this.flags, this.mode, this[ReqWrapSym]);
};

function onWSWrite(er, bytes) {
  const stream = this.stream;
  if (er) {
    if (stream.autoClose) {
      stream.destroy();
    }
    return stream[WriteStreamCbSym](er);
  }
  stream.bytesWritten += bytes;
  stream[WriteStreamCbSym]();
}

WriteStream.prototype._write = function(data, encoding, cb) {
  if (!(data instanceof Buffer))
    return this.emit('error', new Error('Invalid data'));

  if (typeof this.fd !== 'number') {
    return this.once('open', () => {
      this._write(data, encoding, cb);
    });
  }
  this[WriteStreamCbSym] = cb;
  binding.writeBuffer(this.fd, data, 0, data.length, this.pos,
                      this[ReqWrapSym]);

  if (this.pos !== undefined)
    this.pos += data.length;
};

function onWSWritev(er, bytes) {
  const stream = this.stream;
  if (er) {
    stream.destroy();
    return stream[WriteStreamCbSym](er);
  }
  stream.bytesWritten += bytes;
  stream[WriteStreamCbSym]();
}

WriteStream.prototype._writev = function(data, cb) {
  if (typeof this.fd !== 'number') {
    return this.once('open', () => {
      this._writev(data, cb);
    });
  }

  // Lazily initialize the second FSReqWrap
  if (this[ReqWrapEvSym] === undefined) {
    const req = new FSReqWrap();
    req.stream = this;
    req.oncomplete = onWSWritev;
    this[ReqWrapEvSym] = req;
  }

  const chunks = new Array(data.length);
  var size = 0;

  for (var i = 0; i < data.length; i++) {
    const chunk = data[i].chunk;

    chunks[i] = chunk;
    size += chunk.length;
  }

  this[WriteStreamCbSym] = cb;
  binding.writeBuffers(this.fd, chunks, this.pos, this[ReqWrapEvSym]);

  if (this.pos !== undefined)
    this.pos += size;
};

WriteStream.prototype._destroy = ReadStream.prototype._destroy;
WriteStream.prototype.close = ReadStream.prototype.close;

// There is no shutdown() for files.
WriteStream.prototype.destroySoon = WriteStream.prototype.end;

// SyncWriteStream is internal. DO NOT USE.
// This undocumented API was never intended to be made public.
var SyncWriteStream = InternalSyncWriteStream;
Object.defineProperty(fs, 'SyncWriteStream', {
  configurable: true,
  get: internalUtil.deprecate(() => SyncWriteStream,
                              'fs.SyncWriteStream is deprecated.', 'DEP0061'),
  set: internalUtil.deprecate((val) => { SyncWriteStream = val; },
                              'fs.SyncWriteStream is deprecated.', 'DEP0061')
});
