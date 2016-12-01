// Maintainers, keep in mind that ES1-style octal literals (`0666`) are not
// allowed in strict mode. Use ES6-style octal literals instead (`0o666`).

'use strict';

const constants = process.binding('constants').fs;
const util = require('util');
const pathModule = require('path');

const binding = process.binding('fs');
const fs = exports;
const Buffer = require('buffer').Buffer;
const Stream = require('stream').Stream;
const EventEmitter = require('events');
const FSReqWrap = binding.FSReqWrap;
const FSEvent = process.binding('fs_event_wrap').FSEvent;
const internalFS = require('internal/fs');
const assertEncoding = internalFS.assertEncoding;
const stringToFlags = internalFS.stringToFlags;
const SyncWriteStream = internalFS.SyncWriteStream;

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

function getOptions(options, defaultOptions) {
  if (options === null || options === undefined ||
      typeof options === 'function') {
    return defaultOptions;
  }

  if (typeof options === 'string') {
    defaultOptions = util._extend({}, defaultOptions);
    defaultOptions.encoding = options;
    options = defaultOptions;
  } else if (typeof options !== 'object') {
    throw new TypeError('"options" must be a string or an object, got ' +
                        typeof options + ' instead.');
  }

  if (options.encoding !== 'buffer')
    assertEncoding(options.encoding);
  return options;
}

function copyObject(source) {
  const target = {};
  for (const key in source)
    target[key] = source[key];
  return target;
}

function rethrow() {
  // TODO(thefourtheye) Throw error instead of warning in major version > 7
  process.emitWarning(
    'Calling an asynchronous function without callback is deprecated.',
    'DeprecationWarning',
    rethrow
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
    throw new TypeError('"callback" argument must be a function');
  }

  return function() {
    return cb.apply(null, arguments);
  };
}

function nullCheck(path, callback) {
  if (('' + path).indexOf('\u0000') !== -1) {
    var er = new Error('Path must be a string without null bytes');
    er.code = 'ENOENT';
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

// Static method to set the stats properties on a Stats object.
fs.Stats = function(
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
    birthtim_msec) {
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
  this.atime = new Date(atim_msec);
  this.mtime = new Date(mtim_msec);
  this.ctime = new Date(ctim_msec);
  this.birthtime = new Date(birthtim_msec);
};

// Create a C++ binding to the function which creates a Stats object.
binding.FSInitialize(fs.Stats);

fs.Stats.prototype._checkModeProperty = function(property) {
  return ((this.mode & constants.S_IFMT) === property);
};

fs.Stats.prototype.isDirectory = function() {
  return this._checkModeProperty(constants.S_IFDIR);
};

fs.Stats.prototype.isFile = function() {
  return this._checkModeProperty(constants.S_IFREG);
};

fs.Stats.prototype.isBlockDevice = function() {
  return this._checkModeProperty(constants.S_IFBLK);
};

fs.Stats.prototype.isCharacterDevice = function() {
  return this._checkModeProperty(constants.S_IFCHR);
};

fs.Stats.prototype.isSymbolicLink = function() {
  return this._checkModeProperty(constants.S_IFLNK);
};

fs.Stats.prototype.isFIFO = function() {
  return this._checkModeProperty(constants.S_IFIFO);
};

fs.Stats.prototype.isSocket = function() {
  return this._checkModeProperty(constants.S_IFSOCK);
};

// Don't allow mode to accidentally be overwritten.
['F_OK', 'R_OK', 'W_OK', 'X_OK'].forEach(function(key) {
  Object.defineProperty(fs, key, {
    enumerable: true, value: constants[key] || 0, writable: false
  });
});

fs.access = function(path, mode, callback) {
  if (typeof mode === 'function') {
    callback = mode;
    mode = fs.F_OK;
  } else if (typeof callback !== 'function') {
    throw new TypeError('"callback" argument must be a function');
  }

  if (!nullCheck(path, callback))
    return;

  mode = mode | 0;
  var req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.access(pathModule._makeLong(path), mode, req);
};

fs.accessSync = function(path, mode) {
  nullCheck(path);

  if (mode === undefined)
    mode = fs.F_OK;
  else
    mode = mode | 0;

  binding.access(pathModule._makeLong(path), mode);
};

fs.exists = function(path, callback) {
  if (!nullCheck(path, cb)) return;
  var req = new FSReqWrap();
  req.oncomplete = cb;
  binding.stat(pathModule._makeLong(path), req);
  function cb(err, stats) {
    if (callback) callback(err ? false : true);
  }
};

fs.existsSync = function(path) {
  try {
    nullCheck(path);
    binding.stat(pathModule._makeLong(path));
    return true;
  } catch (e) {
    return false;
  }
};

fs.readFile = function(path, options, callback) {
  callback = maybeCallback(arguments[arguments.length - 1]);
  options = getOptions(options, { flag: 'r' });

  if (!nullCheck(path, callback))
    return;

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

  binding.open(pathModule._makeLong(path),
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

function readFileAfterStat(err, st) {
  var context = this.context;

  if (err)
    return context.close(err);

  var size = context.size = st.isFile() ? st.size : 0;

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
  var st;
  try {
    st = fs.fstatSync(fd);
    threw = false;
  } finally {
    if (threw && !isUserFd) fs.closeSync(fd);
  }
  return st;
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

  var st = tryStatSync(fd, isUserFd);
  var size = st.isFile() ? st.size : 0;
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
  var req = new FSReqWrap();
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
    return modeNum(def);
  return undefined;
}

fs.open = function(path, flags, mode, callback_) {
  var callback = makeCallback(arguments[arguments.length - 1]);
  mode = modeNum(mode, 0o666);

  if (!nullCheck(path, callback)) return;

  var req = new FSReqWrap();
  req.oncomplete = callback;

  binding.open(pathModule._makeLong(path),
               stringToFlags(flags),
               mode,
               req);
};

fs.openSync = function(path, flags, mode) {
  mode = modeNum(mode, 0o666);
  nullCheck(path);
  return binding.open(pathModule._makeLong(path), stringToFlags(flags), mode);
};

var readWarned = false;
fs.read = function(fd, buffer, offset, length, position, callback) {
  if (!(buffer instanceof Buffer)) {
    // legacy string interface (fd, length, position, encoding, callback)
    if (!readWarned) {
      readWarned = true;
      process.emitWarning(
        'fs.read\'s legacy String interface is deprecated. Use the Buffer ' +
        'API as mentioned in the documentation instead.',
        'DeprecationWarning');
    }

    const cb = arguments[4];
    const encoding = arguments[3];

    assertEncoding(encoding);

    position = arguments[2];
    length = arguments[1];
    buffer = Buffer.allocUnsafe(length);
    offset = 0;

    callback = function(err, bytesRead) {
      if (!cb) return;
      if (err) return cb(err);

      if (bytesRead > 0) {
        tryToStringWithEnd(buffer, encoding, bytesRead, cb);
      } else {
        (cb)(err, '', bytesRead);
      }
    };
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

  var req = new FSReqWrap();
  req.oncomplete = wrapper;

  binding.read(fd, buffer, offset, length, position, req);
};

function tryToStringWithEnd(buf, encoding, end, callback) {
  var e;
  try {
    buf = buf.toString(encoding, 0, end);
  } catch (err) {
    e = err;
  }
  callback(e, buf, end);
}

var readSyncWarned = false;
fs.readSync = function(fd, buffer, offset, length, position) {
  var legacy = false;
  var encoding;

  if (!(buffer instanceof Buffer)) {
    // legacy string interface (fd, length, position, encoding, callback)
    if (!readSyncWarned) {
      readSyncWarned = true;
      process.emitWarning(
        'fs.readSync\'s legacy String interface is deprecated. Use the ' +
        'Buffer API as mentioned in the documentation instead.',
        'DeprecationWarning');
    }
    legacy = true;
    encoding = arguments[3];

    assertEncoding(encoding);

    position = arguments[2];
    length = arguments[1];
    buffer = Buffer.allocUnsafe(length);

    offset = 0;
  }

  if (length === 0) {
    if (legacy) {
      return ['', 0];
    } else {
      return 0;
    }
  }

  var r = binding.read(fd, buffer, offset, length, position);
  if (!legacy) {
    return r;
  }

  var str = (r > 0) ? buffer.toString(encoding, 0, r) : '';
  return [str, r];
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

  var req = new FSReqWrap();
  req.oncomplete = wrapper;

  if (buffer instanceof Buffer) {
    callback = maybeCallback(callback || position || length || offset);
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

// usage:
//  fs.writeSync(fd, buffer[, offset[, length[, position]]]);
// OR
//  fs.writeSync(fd, string[, position[, encoding]]);
fs.writeSync = function(fd, buffer, offset, length, position) {
  if (buffer instanceof Buffer) {
    if (position === undefined)
      position = null;
    if (typeof offset !== 'number')
      offset = 0;
    if (typeof length !== 'number')
      length = buffer.length - offset;
    return binding.writeBuffer(fd, buffer, offset, length, position);
  }
  if (typeof buffer !== 'string')
    buffer += '';
  if (offset === undefined)
    offset = null;
  return binding.writeString(fd, buffer, offset, length, position);
};

fs.rename = function(oldPath, newPath, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(oldPath, callback)) return;
  if (!nullCheck(newPath, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.rename(pathModule._makeLong(oldPath),
                 pathModule._makeLong(newPath),
                 req);
};

fs.renameSync = function(oldPath, newPath) {
  nullCheck(oldPath);
  nullCheck(newPath);
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

fs.ftruncate = function(fd, len, callback) {
  if (typeof len === 'function') {
    callback = len;
    len = 0;
  } else if (len === undefined) {
    len = 0;
  }
  var req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
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
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.rmdir(pathModule._makeLong(path), req);
};

fs.rmdirSync = function(path) {
  nullCheck(path);
  return binding.rmdir(pathModule._makeLong(path));
};

fs.fdatasync = function(fd, callback) {
  var req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fdatasync(fd, req);
};

fs.fdatasyncSync = function(fd) {
  return binding.fdatasync(fd);
};

fs.fsync = function(fd, callback) {
  var req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fsync(fd, req);
};

fs.fsyncSync = function(fd) {
  return binding.fsync(fd);
};

fs.mkdir = function(path, mode, callback) {
  if (typeof mode === 'function') callback = mode;
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.mkdir(pathModule._makeLong(path),
                modeNum(mode, 0o777),
                req);
};

fs.mkdirSync = function(path, mode) {
  nullCheck(path);
  return binding.mkdir(pathModule._makeLong(path),
                       modeNum(mode, 0o777));
};

fs.readdir = function(path, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readdir(pathModule._makeLong(path), options.encoding, req);
};

fs.readdirSync = function(path, options) {
  options = getOptions(options, {});
  nullCheck(path);
  return binding.readdir(pathModule._makeLong(path), options.encoding);
};

fs.fstat = function(fd, callback) {
  var req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fstat(fd, req);
};

fs.lstat = function(path, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.lstat(pathModule._makeLong(path), req);
};

fs.stat = function(path, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.stat(pathModule._makeLong(path), req);
};

fs.fstatSync = function(fd) {
  return binding.fstat(fd);
};

fs.lstatSync = function(path) {
  nullCheck(path);
  return binding.lstat(pathModule._makeLong(path));
};

fs.statSync = function(path) {
  nullCheck(path);
  return binding.stat(pathModule._makeLong(path));
};

fs.readlink = function(path, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readlink(pathModule._makeLong(path), options.encoding, req);
};

fs.readlinkSync = function(path, options) {
  options = getOptions(options, {});
  nullCheck(path);
  return binding.readlink(pathModule._makeLong(path), options.encoding);
};

function preprocessSymlinkDestination(path, type, linkPath) {
  if (!isWindows) {
    // No preprocessing is needed on Unix.
    return path;
  } else if (type === 'junction') {
    // Junctions paths need to be absolute and \\?\-prefixed.
    // A relative target is relative to the link's parent directory.
    path = pathModule.resolve(linkPath, '..', path);
    return pathModule._makeLong(path);
  } else {
    // Windows symlinks don't tolerate forward slashes.
    return ('' + path).replace(/\//g, '\\');
  }
}

fs.symlink = function(target, path, type_, callback_) {
  var type = (typeof type_ === 'string' ? type_ : null);
  var callback = makeCallback(arguments[arguments.length - 1]);

  if (!nullCheck(target, callback)) return;
  if (!nullCheck(path, callback)) return;

  var req = new FSReqWrap();
  req.oncomplete = callback;

  binding.symlink(preprocessSymlinkDestination(target, type, path),
                  pathModule._makeLong(path),
                  type,
                  req);
};

fs.symlinkSync = function(target, path, type) {
  type = (typeof type === 'string' ? type : null);

  nullCheck(target);
  nullCheck(path);

  return binding.symlink(preprocessSymlinkDestination(target, type, path),
                         pathModule._makeLong(path),
                         type);
};

fs.link = function(existingPath, newPath, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(existingPath, callback)) return;
  if (!nullCheck(newPath, callback)) return;

  var req = new FSReqWrap();
  req.oncomplete = callback;

  binding.link(pathModule._makeLong(existingPath),
               pathModule._makeLong(newPath),
               req);
};

fs.linkSync = function(existingPath, newPath) {
  nullCheck(existingPath);
  nullCheck(newPath);
  return binding.link(pathModule._makeLong(existingPath),
                      pathModule._makeLong(newPath));
};

fs.unlink = function(path, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.unlink(pathModule._makeLong(path), req);
};

fs.unlinkSync = function(path) {
  nullCheck(path);
  return binding.unlink(pathModule._makeLong(path));
};

fs.fchmod = function(fd, mode, callback) {
  var req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fchmod(fd, modeNum(mode), req);
};

fs.fchmodSync = function(fd, mode) {
  return binding.fchmod(fd, modeNum(mode));
};

if (constants.hasOwnProperty('O_SYMLINK')) {
  fs.lchmod = function(path, mode, callback) {
    callback = maybeCallback(callback);
    fs.open(path, constants.O_WRONLY | constants.O_SYMLINK, function(err, fd) {
      if (err) {
        callback(err);
        return;
      }
      // prefer to return the chmod error, if one occurs,
      // but still try to close, and report closing errors if they occur.
      fs.fchmod(fd, mode, function(err) {
        fs.close(fd, function(err2) {
          callback(err || err2);
        });
      });
    });
  };

  fs.lchmodSync = function(path, mode) {
    var fd = fs.openSync(path, constants.O_WRONLY | constants.O_SYMLINK);

    // prefer to return the chmod error, if one occurs,
    // but still try to close, and report closing errors if they occur.
    var err, err2, ret;
    try {
      ret = fs.fchmodSync(fd, mode);
    } catch (er) {
      err = er;
    }
    try {
      fs.closeSync(fd);
    } catch (er) {
      err2 = er;
    }
    if (err || err2) throw (err || err2);
    return ret;
  };
}


fs.chmod = function(path, mode, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.chmod(pathModule._makeLong(path),
                modeNum(mode),
                req);
};

fs.chmodSync = function(path, mode) {
  nullCheck(path);
  return binding.chmod(pathModule._makeLong(path), modeNum(mode));
};

if (constants.hasOwnProperty('O_SYMLINK')) {
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
  var req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.fchown(fd, uid, gid, req);
};

fs.fchownSync = function(fd, uid, gid) {
  return binding.fchown(fd, uid, gid);
};

fs.chown = function(path, uid, gid, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.chown(pathModule._makeLong(path), uid, gid, req);
};

fs.chownSync = function(path, uid, gid) {
  nullCheck(path);
  return binding.chown(pathModule._makeLong(path), uid, gid);
};

// converts Date or number to a fractional UNIX timestamp
function toUnixTimestamp(time) {
  if (typeof time === 'string' && +time == time) {
    return +time;
  }
  if (typeof time === 'number') {
    if (!Number.isFinite(time) || time < 0) {
      return Date.now() / 1000;
    }
    return time;
  }
  if (util.isDate(time)) {
    // convert to 123.456 UNIX timestamp
    return time.getTime() / 1000;
  }
  throw new Error('Cannot parse time: ' + time);
}

// exported for unit tests, not for public consumption
fs._toUnixTimestamp = toUnixTimestamp;

fs.utimes = function(path, atime, mtime, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.utimes(pathModule._makeLong(path),
                 toUnixTimestamp(atime),
                 toUnixTimestamp(mtime),
                 req);
};

fs.utimesSync = function(path, atime, mtime) {
  nullCheck(path);
  atime = toUnixTimestamp(atime);
  mtime = toUnixTimestamp(mtime);
  binding.utimes(pathModule._makeLong(path), atime, mtime);
};

fs.futimes = function(fd, atime, mtime, callback) {
  atime = toUnixTimestamp(atime);
  mtime = toUnixTimestamp(mtime);
  var req = new FSReqWrap();
  req.oncomplete = makeCallback(callback);
  binding.futimes(fd, atime, mtime, req);
};

fs.futimesSync = function(fd, atime, mtime) {
  atime = toUnixTimestamp(atime);
  mtime = toUnixTimestamp(mtime);
  binding.futimes(fd, atime, mtime);
};

function writeAll(fd, isUserFd, buffer, offset, length, position, callback_) {
  var callback = maybeCallback(arguments[arguments.length - 1]);

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
  callback = maybeCallback(arguments[arguments.length - 1]);
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
    var buffer = (data instanceof Buffer) ?
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

  if (!(data instanceof Buffer)) {
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
  callback = maybeCallback(arguments[arguments.length - 1]);
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
    if (status < 0) {
      self._handle.close();
      const error = !filename ?
          errnoException(status, 'Error watching file for changes:') :
          errnoException(status,
                         `Error watching file ${filename} for changes:`);
      error.filename = filename;
      self.emit('error', error);
    } else {
      self.emit('change', eventType, filename);
    }
  };
}
util.inherits(FSWatcher, EventEmitter);

FSWatcher.prototype.start = function(filename,
                                     persistent,
                                     recursive,
                                     encoding) {
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

  this._handle.onchange = function(current, previous, newStatus) {
    if (oldStatus === -1 &&
        newStatus === -1 &&
        current.nlink === previous.nlink) return;

    oldStatus = newStatus;
    self.emit('change', current, previous);
  };

  this._handle.onstop = function() {
    process.nextTick(emitStop, self);
  };
}
util.inherits(StatWatcher, EventEmitter);


StatWatcher.prototype.start = function(filename, persistent, interval) {
  nullCheck(filename);
  this._handle.start(pathModule._makeLong(filename), persistent, interval);
};


StatWatcher.prototype.stop = function() {
  this._handle.stop();
};


const statWatchers = new Map();

fs.watchFile = function(filename, options, listener) {
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
    throw new Error('"watchFile()" requires a listener function');
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
  nullCheck(filename);
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


// Regexp that finds the next portion of a (partial) path
// result is [base_with_slash, base], e.g. ['somedir/', 'somedir']
const nextPartRe = isWindows ?
  /(.*?)(?:[/\\]+|$)/g :
  /(.*?)(?:[/]+|$)/g;

// Regex to find the device root, including trailing slash. E.g. 'c:\\'.
const splitRootRe = isWindows ?
  /^(?:[a-zA-Z]:|[\\/]{2}[^\\/]+[\\/][^\\/]+)?[\\/]*/ :
  /^[/]*/;

function encodeRealpathResult(result, options, err) {
  if (!options || !options.encoding || options.encoding === 'utf8' || err)
    return result;
  const asBuffer = Buffer.from(result);
  if (options.encoding === 'buffer') {
    return asBuffer;
  } else {
    return asBuffer.toString(options.encoding);
  }
}

fs.realpathSync = function realpathSync(p, options) {
  options = getOptions(options, {});
  nullCheck(p);

  p = p.toString('utf8');
  p = pathModule.resolve(p);

  const seenLinks = {};
  const knownHard = {};
  const cache = options[internalFS.realpathCacheKey];
  const original = p;

  const maybeCachedResult = cache && cache.get(p);
  if (maybeCachedResult) {
    return maybeCachedResult;
  }

  // current character position in p
  var pos;
  // the partial path so far, including a trailing slash if any
  var current;
  // the partial path without a trailing slash (except when pointing at a root)
  var base;
  // the partial path scanned in the previous round, with slash
  var previous;

  start();

  function start() {
    // Skip over roots
    var m = splitRootRe.exec(p);
    pos = m[0].length;
    current = m[0];
    base = m[0];
    previous = '';

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard[base]) {
      fs.lstatSync(base);
      knownHard[base] = true;
    }
  }

  // walk down the path, swapping out linked pathparts for their real
  // values
  // NB: p.length changes.
  while (pos < p.length) {
    // find the next part
    nextPartRe.lastIndex = pos;
    var result = nextPartRe.exec(p);
    previous = current;
    current += result[0];
    base = previous + result[1];
    pos = nextPartRe.lastIndex;

    // continue if not a symlink
    if (knownHard[base] || (cache && cache.get(base) === base)) {
      continue;
    }

    var resolvedLink;
    const maybeCachedResolved = cache && cache.get(base);
    if (maybeCachedResolved) {
      resolvedLink = maybeCachedResolved;
    } else {
      var stat = fs.lstatSync(base);
      if (!stat.isSymbolicLink()) {
        knownHard[base] = true;
        continue;
      }

      // read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      let linkTarget = null;
      let id;
      if (!isWindows) {
        id = `${stat.dev.toString(32)}:${stat.ino.toString(32)}`;
        if (seenLinks.hasOwnProperty(id)) {
          linkTarget = seenLinks[id];
        }
      }
      if (linkTarget === null) {
        fs.statSync(base);
        linkTarget = fs.readlinkSync(base);
      }
      resolvedLink = pathModule.resolve(previous, linkTarget);

      if (cache) cache.set(base, resolvedLink);
      if (!isWindows) seenLinks[id] = linkTarget;
    }

    // resolve the link, then start over
    p = pathModule.resolve(resolvedLink, p.slice(pos));
    start();
  }

  if (cache) cache.set(original, p);
  return encodeRealpathResult(p, options);
};


fs.realpath = function realpath(p, options, callback) {
  callback = maybeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  if (!nullCheck(p, callback))
    return;

  p = p.toString('utf8');
  p = pathModule.resolve(p);

  const seenLinks = {};
  const knownHard = {};

  // current character position in p
  var pos;
  // the partial path so far, including a trailing slash if any
  var current;
  // the partial path without a trailing slash (except when pointing at a root)
  var base;
  // the partial path scanned in the previous round, with slash
  var previous;

  start();

  function start() {
    // Skip over roots
    var m = splitRootRe.exec(p);
    pos = m[0].length;
    current = m[0];
    base = m[0];
    previous = '';

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

  // walk down the path, swapping out linked pathparts for their real
  // values
  function LOOP() {
    // stop if scanned past end of path
    if (pos >= p.length) {
      return callback(null, encodeRealpathResult(p, options));
    }

    // find the next part
    nextPartRe.lastIndex = pos;
    var result = nextPartRe.exec(p);
    previous = current;
    current += result[0];
    base = previous + result[1];
    pos = nextPartRe.lastIndex;

    // continue if not a symlink
    if (knownHard[base]) {
      return process.nextTick(LOOP);
    }

    return fs.lstat(base, gotStat);
  }

  function gotStat(err, stat) {
    if (err) return callback(err);

    // if not a symlink, skip to the next path part
    if (!stat.isSymbolicLink()) {
      knownHard[base] = true;
      return process.nextTick(LOOP);
    }

    // stat & read the link if not read before
    // call gotTarget as soon as the link target is known
    // dev/ino always return 0 on windows, so skip the check.
    let id;
    if (!isWindows) {
      id = `${stat.dev.toString(32)}:${stat.ino.toString(32)}`;
      if (seenLinks.hasOwnProperty(id)) {
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
    start();
  }
};

fs.mkdtemp = function(prefix, options, callback) {
  callback = makeCallback(typeof options === 'function' ? options : callback);
  options = getOptions(options, {});
  if (!prefix || typeof prefix !== 'string')
    throw new TypeError('filename prefix is required');
  if (!nullCheck(prefix, callback)) {
    return;
  }

  var req = new FSReqWrap();
  req.oncomplete = callback;

  binding.mkdtemp(prefix + 'XXXXXX', options.encoding, req);
};


fs.mkdtempSync = function(prefix, options) {
  if (!prefix || typeof prefix !== 'string')
    throw new TypeError('filename prefix is required');
  options = getOptions(options, {});
  nullCheck(prefix);
  return binding.mkdtemp(prefix + 'XXXXXX', options.encoding);
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

  this.path = path;
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
      throw new TypeError('"start" option must be a Number');
    }
    if (this.end === undefined) {
      this.end = Infinity;
    } else if (typeof this.end !== 'number') {
      throw new TypeError('"end" option must be a Number');
    }

    if (this.start > this.end) {
      throw new Error('"start" option must be <= "end" option');
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
  if (typeof this.fd !== 'number')
    return this.once('open', function() {
      this._read(n);
    });

  if (this.destroyed)
    return;

  if (!pool || pool.length - pool.used < kMinPoolSpace) {
    // discard the old pool.
    pool = null;
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


ReadStream.prototype.destroy = function() {
  if (this.destroyed)
    return;
  this.destroyed = true;
  this.close();
};


ReadStream.prototype.close = function(cb) {
  var self = this;
  if (cb)
    this.once('close', cb);
  if (this.closed || typeof this.fd !== 'number') {
    if (typeof this.fd !== 'number') {
      this.once('open', close);
      return;
    }
    return process.nextTick(() => this.emit('close'));
  }
  this.closed = true;
  close();

  function close(fd) {
    fs.close(fd || self.fd, function(er) {
      if (er)
        self.emit('error', er);
      else
        self.emit('close');
    });
    self.fd = null;
  }
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

  Writable.call(this, options);

  this.path = path;
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'w' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.autoClose = options.autoClose === undefined ? true : !!options.autoClose;
  this.pos = undefined;
  this.bytesWritten = 0;

  if (this.start !== undefined) {
    if (typeof this.start !== 'number') {
      throw new TypeError('"start" option must be a Number');
    }
    if (this.start < 0) {
      throw new Error('"start" must be >= zero');
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

  if (typeof this.fd !== 'number')
    return this.once('open', function() {
      this._write(data, encoding, cb);
    });

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
  if (typeof this.fd !== 'number')
    return this.once('open', function() {
      this._writev(data, cb);
    });

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


WriteStream.prototype.destroy = ReadStream.prototype.destroy;
WriteStream.prototype.close = ReadStream.prototype.close;

// There is no shutdown() for files.
WriteStream.prototype.destroySoon = WriteStream.prototype.end;

// SyncWriteStream is internal. DO NOT USE.
// todo(jasnell): "Docs-only" deprecation for now. This was never documented
// so there's no documentation to modify. In the future, add a runtime
// deprecation.
Object.defineProperty(fs, 'SyncWriteStream', {
  configurable: true,
  writable: true,
  value: SyncWriteStream
});
