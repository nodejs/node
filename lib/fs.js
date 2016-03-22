// Maintainers, keep in mind that ES1-style octal literals (`0666`) are not
// allowed in strict mode. Use ES6-style octal literals instead (`0o666`).

'use strict';

const SlowBuffer = require('buffer').SlowBuffer;
const util = require('util');
const pathModule = require('path');

const binding = process.binding('fs');
const constants = require('constants');
const fs = exports;
const Buffer = require('buffer').Buffer;
const Stream = require('stream').Stream;
const EventEmitter = require('events');
const FSReqWrap = binding.FSReqWrap;
const FSEvent = process.binding('fs_event_wrap').FSEvent;

const Readable = Stream.Readable;
const Writable = Stream.Writable;

const kMinPoolSpace = 128;
const kMaxLength = require('buffer').kMaxLength;

const O_APPEND = constants.O_APPEND || 0;
const O_CREAT = constants.O_CREAT || 0;
const O_EXCL = constants.O_EXCL || 0;
const O_RDONLY = constants.O_RDONLY || 0;
const O_RDWR = constants.O_RDWR || 0;
const O_SYNC = constants.O_SYNC || 0;
const O_TRUNC = constants.O_TRUNC || 0;
const O_WRONLY = constants.O_WRONLY || 0;

const isWindows = process.platform === 'win32';

const DEBUG = process.env.NODE_DEBUG && /fs/.test(process.env.NODE_DEBUG);
const errnoException = util._errnoException;

function throwOptionsError(options) {
  throw new TypeError('Expected options to be either an object or a string, ' +
    'but got ' + typeof options + ' instead');
}

function rethrow() {
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
    throw new TypeError('callback must be a function');
  }

  return function() {
    return cb.apply(null, arguments);
  };
}

function assertEncoding(encoding) {
  if (encoding && !Buffer.isEncoding(encoding)) {
    throw new Error('Unknown encoding: ' + encoding);
  }
}

function nullCheck(path, callback) {
  if (('' + path).indexOf('\u0000') !== -1) {
    var er = new Error('Path must be a string without null bytes.');
    er.code = 'ENOENT';
    if (typeof callback !== 'function')
      throw er;
    process.nextTick(callback, er);
    return false;
  }
  return true;
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
    throw new TypeError('callback must be a function');
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

fs.readFile = function(path, options, callback_) {
  var callback = maybeCallback(arguments[arguments.length - 1]);

  if (!options || typeof options === 'function') {
    options = { encoding: null, flag: 'r' };
  } else if (typeof options === 'string') {
    options = { encoding: options, flag: 'r' };
  } else if (typeof options !== 'object') {
    throwOptionsError(options);
  }

  var encoding = options.encoding;
  assertEncoding(encoding);

  var flag = options.flag || 'r';

  if (!nullCheck(path, callback))
    return;

  var context = new ReadFileContext(callback, encoding);
  var req = new FSReqWrap();
  req.context = context;
  req.oncomplete = readFileAfterOpen;

  binding.open(pathModule._makeLong(path),
               stringToFlags(flag),
               0o666,
               req);
};

const kReadFileBufferLength = 8 * 1024;

function ReadFileContext(callback, encoding) {
  this.fd = undefined;
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
    buffer = this.buffer = new SlowBuffer(kReadFileBufferLength);
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

  context.buffer = new SlowBuffer(size);
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

  if (context.err)
    return callback(context.err);

  if (context.size === 0)
    buffer = Buffer.concat(context.buffers, context.pos);
  else if (context.pos < context.size)
    buffer = context.buffer.slice(0, context.pos);
  else
    buffer = context.buffer;

  if (context.encoding)
    buffer = buffer.toString(context.encoding);

  callback(err, buffer);
}


fs.readFileSync = function(path, options) {
  if (!options) {
    options = { encoding: null, flag: 'r' };
  } else if (typeof options === 'string') {
    options = { encoding: options, flag: 'r' };
  } else if (typeof options !== 'object') {
    throwOptionsError(options);
  }

  var encoding = options.encoding;
  assertEncoding(encoding);

  var flag = options.flag || 'r';
  var fd = fs.openSync(path, flag, 0o666);

  var st;
  var size;
  var threw = true;
  try {
    st = fs.fstatSync(fd);
    size = st.isFile() ? st.size : 0;
    threw = false;
  } finally {
    if (threw) fs.closeSync(fd);
  }

  var pos = 0;
  var buffer; // single buffer with file data
  var buffers; // list for when size is unknown

  if (size === 0) {
    buffers = [];
  } else {
    threw = true;
    try {
      buffer = new Buffer(size);
      threw = false;
    } finally {
      if (threw) fs.closeSync(fd);
    }
  }

  var done = false;
  var bytesRead;

  while (!done) {
    threw = true;
    try {
      if (size !== 0) {
        bytesRead = fs.readSync(fd, buffer, pos, size - pos);
      } else {
        // the kernel lies about many files.
        // Go ahead and try to read some bytes.
        buffer = new Buffer(8192);
        bytesRead = fs.readSync(fd, buffer, 0, 8192);
        if (bytesRead) {
          buffers.push(buffer.slice(0, bytesRead));
        }
      }
      threw = false;
    } finally {
      if (threw) fs.closeSync(fd);
    }

    pos += bytesRead;
    done = (bytesRead === 0) || (size !== 0 && pos >= size);
  }

  fs.closeSync(fd);

  if (size === 0) {
    // data was collected into the buffers list.
    buffer = Buffer.concat(buffers, pos);
  } else if (pos < size) {
    buffer = buffer.slice(0, pos);
  }

  if (encoding) buffer = buffer.toString(encoding);
  return buffer;
};


// Used by binding.open and friends
function stringToFlags(flag) {
  // Only mess with strings
  if (typeof flag !== 'string') {
    return flag;
  }

  switch (flag) {
    case 'r' : return O_RDONLY;
    case 'rs' : // fall through
    case 'sr' : return O_RDONLY | O_SYNC;
    case 'r+' : return O_RDWR;
    case 'rs+' : // fall through
    case 'sr+' : return O_RDWR | O_SYNC;

    case 'w' : return O_TRUNC | O_CREAT | O_WRONLY;
    case 'wx' : // fall through
    case 'xw' : return O_TRUNC | O_CREAT | O_WRONLY | O_EXCL;

    case 'w+' : return O_TRUNC | O_CREAT | O_RDWR;
    case 'wx+': // fall through
    case 'xw+': return O_TRUNC | O_CREAT | O_RDWR | O_EXCL;

    case 'a' : return O_APPEND | O_CREAT | O_WRONLY;
    case 'ax' : // fall through
    case 'xa' : return O_APPEND | O_CREAT | O_WRONLY | O_EXCL;

    case 'a+' : return O_APPEND | O_CREAT | O_RDWR;
    case 'ax+': // fall through
    case 'xa+': return O_APPEND | O_CREAT | O_RDWR | O_EXCL;
  }

  throw new Error('Unknown file open flag: ' + flag);
}

// exported but hidden, only used by test/simple/test-fs-open-flags.js
Object.defineProperty(exports, '_stringToFlags', {
  enumerable: false,
  value: stringToFlags
});


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

fs.read = function(fd, buffer, offset, length, position, callback) {
  if (!(buffer instanceof Buffer)) {
    // legacy string interface (fd, length, position, encoding, callback)
    const cb = arguments[4];
    const encoding = arguments[3];

    assertEncoding(encoding);

    position = arguments[2];
    length = arguments[1];
    buffer = new Buffer(length);
    offset = 0;

    callback = function(err, bytesRead) {
      if (!cb) return;

      var str = (bytesRead > 0) ? buffer.toString(encoding, 0, bytesRead) : '';

      (cb)(err, str, bytesRead);
    };
  }

  function wrapper(err, bytesRead) {
    // Retain a reference to buffer so that it can't be GC'ed too soon.
    callback && callback(err, bytesRead || 0, buffer);
  }

  var req = new FSReqWrap();
  req.oncomplete = wrapper;

  binding.read(fd, buffer, offset, length, position, req);
};

fs.readSync = function(fd, buffer, offset, length, position) {
  var legacy = false;
  var encoding;

  if (!(buffer instanceof Buffer)) {
    // legacy string interface (fd, length, position, encoding, callback)
    legacy = true;
    encoding = arguments[3];

    assertEncoding(encoding);

    position = arguments[2];
    length = arguments[1];
    buffer = new Buffer(length);

    offset = 0;
  }

  var r = binding.read(fd, buffer, offset, length, position);
  if (!legacy) {
    return r;
  }

  var str = (r > 0) ? buffer.toString(encoding, 0, r) : '';
  return [str, r];
};

// usage:
//  fs.write(fd, buffer, offset, length[, position], callback);
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
    // if no position is passed then assume null
    if (typeof position === 'function') {
      callback = position;
      position = null;
    }
    callback = maybeCallback(callback);
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
//  fs.writeSync(fd, buffer, offset, length[, position]);
// OR
//  fs.writeSync(fd, string[, position[, encoding]]);
fs.writeSync = function(fd, buffer, offset, length, position) {
  if (buffer instanceof Buffer) {
    if (position === undefined)
      position = null;
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
    req.oncomplete = function ftruncateCb(er) {
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

fs.readdir = function(path, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readdir(pathModule._makeLong(path), req);
};

fs.readdirSync = function(path) {
  nullCheck(path);
  return binding.readdir(pathModule._makeLong(path));
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

fs.readlink = function(path, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  var req = new FSReqWrap();
  req.oncomplete = callback;
  binding.readlink(pathModule._makeLong(path), req);
};

fs.readlinkSync = function(path) {
  nullCheck(path);
  return binding.readlink(pathModule._makeLong(path));
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

fs.link = function(srcpath, dstpath, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(srcpath, callback)) return;
  if (!nullCheck(dstpath, callback)) return;

  var req = new FSReqWrap();
  req.oncomplete = callback;

  binding.link(pathModule._makeLong(srcpath),
               pathModule._makeLong(dstpath),
               req);
};

fs.linkSync = function(srcpath, dstpath) {
  nullCheck(srcpath);
  nullCheck(dstpath);
  return binding.link(pathModule._makeLong(srcpath),
                      pathModule._makeLong(dstpath));
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

function writeAll(fd, buffer, offset, length, position, callback_) {
  var callback = maybeCallback(arguments[arguments.length - 1]);

  // write(fd, buffer, offset, length, position, callback)
  fs.write(fd, buffer, offset, length, position, function(writeErr, written) {
    if (writeErr) {
      fs.close(fd, function() {
        callback(writeErr);
      });
    } else {
      if (written === length) {
        fs.close(fd, callback);
      } else {
        offset += written;
        length -= written;
        if (position !== null) {
          position += written;
        }
        writeAll(fd, buffer, offset, length, position, callback);
      }
    }
  });
}

fs.writeFile = function(path, data, options, callback_) {
  var callback = maybeCallback(arguments[arguments.length - 1]);

  if (!options || typeof options === 'function') {
    options = { encoding: 'utf8', mode: 0o666, flag: 'w' };
  } else if (typeof options === 'string') {
    options = { encoding: options, mode: 0o666, flag: 'w' };
  } else if (typeof options !== 'object') {
    throwOptionsError(options);
  }

  assertEncoding(options.encoding);

  var flag = options.flag || 'w';
  fs.open(path, flag, options.mode, function(openErr, fd) {
    if (openErr) {
      callback(openErr);
    } else {
      var buffer = (data instanceof Buffer) ? data : new Buffer('' + data,
          options.encoding || 'utf8');
      var position = /a/.test(flag) ? null : 0;
      writeAll(fd, buffer, 0, buffer.length, position, callback);
    }
  });
};

fs.writeFileSync = function(path, data, options) {
  if (!options) {
    options = { encoding: 'utf8', mode: 0o666, flag: 'w' };
  } else if (typeof options === 'string') {
    options = { encoding: options, mode: 0o666, flag: 'w' };
  } else if (typeof options !== 'object') {
    throwOptionsError(options);
  }

  assertEncoding(options.encoding);

  var flag = options.flag || 'w';
  var fd = fs.openSync(path, flag, options.mode);
  if (!(data instanceof Buffer)) {
    data = new Buffer('' + data, options.encoding || 'utf8');
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
    fs.closeSync(fd);
  }
};

fs.appendFile = function(path, data, options, callback_) {
  var callback = maybeCallback(arguments[arguments.length - 1]);

  if (!options || typeof options === 'function') {
    options = { encoding: 'utf8', mode: 0o666, flag: 'a' };
  } else if (typeof options === 'string') {
    options = { encoding: options, mode: 0o666, flag: 'a' };
  } else if (typeof options !== 'object') {
    throwOptionsError(options);
  }

  if (!options.flag)
    options = util._extend({ flag: 'a' }, options);
  fs.writeFile(path, data, options, callback);
};

fs.appendFileSync = function(path, data, options) {
  if (!options) {
    options = { encoding: 'utf8', mode: 0o666, flag: 'a' };
  } else if (typeof options === 'string') {
    options = { encoding: options, mode: 0o666, flag: 'a' };
  } else if (typeof options !== 'object') {
    throwOptionsError(options);
  }
  if (!options.flag)
    options = util._extend({ flag: 'a' }, options);

  fs.writeFileSync(path, data, options);
};

function FSWatcher() {
  EventEmitter.call(this);

  var self = this;
  this._handle = new FSEvent();
  this._handle.owner = this;

  this._handle.onchange = function(status, event, filename) {
    if (status < 0) {
      self._handle.close();
      const error = errnoException(status, `watch ${filename}`);
      error.filename = filename;
      self.emit('error', error);
    } else {
      self.emit('change', event, filename);
    }
  };
}
util.inherits(FSWatcher, EventEmitter);

FSWatcher.prototype.start = function(filename, persistent, recursive) {
  nullCheck(filename);
  var err = this._handle.start(pathModule._makeLong(filename),
                               persistent,
                               recursive);
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

fs.watch = function(filename) {
  nullCheck(filename);
  var watcher;
  var options;
  var listener;

  if (arguments[1] !== null && typeof arguments[1] === 'object') {
    options = arguments[1];
    listener = arguments[2];
  } else {
    options = {};
    listener = arguments[1];
  }

  if (options.persistent === undefined) options.persistent = true;
  if (options.recursive === undefined) options.recursive = false;

  watcher = new FSWatcher();
  watcher.start(filename, options.persistent, options.recursive);

  if (listener) {
    watcher.addListener('change', listener);
  }

  return watcher;
};


// Stat Change Watchers

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
    self.emit('stop');
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
    throw new Error('watchFile requires a listener function');
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

// Regexp that finds the next partion of a (partial) path
// result is [base_with_slash, base], e.g. ['somedir/', 'somedir']
const nextPartRe = isWindows ?
  /(.*?)(?:[\/\\]+|$)/g :
  /(.*?)(?:[\/]+|$)/g;

// Regex to find the device root, including trailing slash. E.g. 'c:\\'.
const splitRootRe = isWindows ?
  /^(?:[a-zA-Z]:|[\\\/]{2}[^\\\/]+[\\\/][^\\\/]+)?[\\\/]*/ :
  /^[\/]*/;

fs.realpathSync = function realpathSync(p, cache) {
  // make p is absolute
  p = pathModule.resolve(p);

  if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
    return cache[p];
  }

  const original = p;
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
    if (knownHard[base] || (cache && cache[base] === base)) {
      continue;
    }

    var resolvedLink;
    if (cache && Object.prototype.hasOwnProperty.call(cache, base)) {
      // some known symbolic link.  no need to stat again.
      resolvedLink = cache[base];
    } else {
      var stat = fs.lstatSync(base);
      if (!stat.isSymbolicLink()) {
        knownHard[base] = true;
        if (cache) cache[base] = base;
        continue;
      }

      // read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      var linkTarget = null;
      if (!isWindows) {
        var id = stat.dev.toString(32) + ':' + stat.ino.toString(32);
        if (seenLinks.hasOwnProperty(id)) {
          linkTarget = seenLinks[id];
        }
      }
      if (linkTarget === null) {
        fs.statSync(base);
        linkTarget = fs.readlinkSync(base);
      }
      resolvedLink = pathModule.resolve(previous, linkTarget);
      // track this, if given a cache.
      if (cache) cache[base] = resolvedLink;
      if (!isWindows) seenLinks[id] = linkTarget;
    }

    // resolve the link, then start over
    p = pathModule.resolve(resolvedLink, p.slice(pos));
    start();
  }

  if (cache) cache[original] = p;

  return p;
};


fs.realpath = function realpath(p, cache, cb) {
  if (typeof cb !== 'function') {
    cb = maybeCallback(cache);
    cache = null;
  }

  // make p is absolute
  p = pathModule.resolve(p);

  if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
    return process.nextTick(cb.bind(null, null, cache[p]));
  }

  const original = p;
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
        if (err) return cb(err);
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
      if (cache) cache[original] = p;
      return cb(null, p);
    }

    // find the next part
    nextPartRe.lastIndex = pos;
    var result = nextPartRe.exec(p);
    previous = current;
    current += result[0];
    base = previous + result[1];
    pos = nextPartRe.lastIndex;

    // continue if not a symlink
    if (knownHard[base] || (cache && cache[base] === base)) {
      return process.nextTick(LOOP);
    }

    if (cache && Object.prototype.hasOwnProperty.call(cache, base)) {
      // known symbolic link.  no need to stat again.
      return gotResolvedLink(cache[base]);
    }

    return fs.lstat(base, gotStat);
  }

  function gotStat(err, stat) {
    if (err) return cb(err);

    // if not a symlink, skip to the next path part
    if (!stat.isSymbolicLink()) {
      knownHard[base] = true;
      if (cache) cache[base] = base;
      return process.nextTick(LOOP);
    }

    // stat & read the link if not read before
    // call gotTarget as soon as the link target is known
    // dev/ino always return 0 on windows, so skip the check.
    if (!isWindows) {
      var id = stat.dev.toString(32) + ':' + stat.ino.toString(32);
      if (seenLinks.hasOwnProperty(id)) {
        return gotTarget(null, seenLinks[id], base);
      }
    }
    fs.stat(base, function(err) {
      if (err) return cb(err);

      fs.readlink(base, function(err, target) {
        if (!isWindows) seenLinks[id] = target;
        gotTarget(err, target);
      });
    });
  }

  function gotTarget(err, target, base) {
    if (err) return cb(err);

    var resolvedLink = pathModule.resolve(previous, target);
    if (cache) cache[base] = resolvedLink;
    gotResolvedLink(resolvedLink);
  }

  function gotResolvedLink(resolvedLink) {
    // resolve the link, then start over
    p = pathModule.resolve(resolvedLink, p.slice(pos));
    start();
  }
};


var pool;

function allocNewPool(poolSize) {
  pool = new Buffer(poolSize);
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

  if (options === undefined)
    options = {};
  else if (typeof options === 'string')
    options = { encoding: options };
  else if (options === null || typeof options !== 'object')
    throw new TypeError('options must be a string or an object');

  // a little bit bigger buffer and water marks by default
  options = Object.create(options);
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

  if (this.start !== undefined) {
    if (typeof this.start !== 'number') {
      throw new TypeError('start must be a Number');
    }
    if (this.end === undefined) {
      this.end = Infinity;
    } else if (typeof this.end !== 'number') {
      throw new TypeError('end must be a Number');
    }

    if (this.start > this.end) {
      throw new Error('start must be <= end');
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
      if (bytesRead > 0)
        b = thisPool.slice(start, start + bytesRead);

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

  if (options === undefined)
    options = {};
  else if (typeof options === 'string')
    options = { encoding: options };
  else if (options === null || typeof options !== 'object')
    throw new TypeError('options must be a string or an object');

  options = Object.create(options);

  Writable.call(this, options);

  this.path = path;
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'w' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.pos = undefined;
  this.bytesWritten = 0;

  if (this.start !== undefined) {
    if (typeof this.start !== 'number') {
      throw new TypeError('start must be a Number');
    }
    if (this.start < 0) {
      throw new Error('start must be >= zero');
    }

    this.pos = this.start;
  }

  if (options.encoding)
    this.setDefaultEncoding(options.encoding);

  if (typeof this.fd !== 'number')
    this.open();

  // dispose on finish.
  this.once('finish', this.close);
}

fs.FileWriteStream = fs.WriteStream; // support the legacy name


WriteStream.prototype.open = function() {
  fs.open(this.path, this.flags, this.mode, function(er, fd) {
    if (er) {
      this.destroy();
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
      self.destroy();
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
// Temporary hack for process.stdout and process.stderr when piped to files.
function SyncWriteStream(fd, options) {
  Stream.call(this);

  options = options || {};

  this.fd = fd;
  this.writable = true;
  this.readable = false;
  this.autoClose = options.autoClose === undefined ? true : options.autoClose;
}

util.inherits(SyncWriteStream, Stream);


// Export
Object.defineProperty(fs, 'SyncWriteStream', {
  configurable: true,
  writable: true,
  value: SyncWriteStream
});

SyncWriteStream.prototype.write = function(data, arg1, arg2) {
  var encoding, cb;

  // parse arguments
  if (arg1) {
    if (typeof arg1 === 'string') {
      encoding = arg1;
      cb = arg2;
    } else if (typeof arg1 === 'function') {
      cb = arg1;
    } else {
      throw new Error('bad arg');
    }
  }
  assertEncoding(encoding);

  // Change strings to buffers. SLOW
  if (typeof data === 'string') {
    data = new Buffer(data, encoding);
  }

  fs.writeSync(this.fd, data, 0, data.length);

  if (cb) {
    process.nextTick(cb);
  }

  return true;
};


SyncWriteStream.prototype.end = function(data, arg1, arg2) {
  if (data) {
    this.write(data, arg1, arg2);
  }
  this.destroy();
};


SyncWriteStream.prototype.destroy = function() {
  if (this.autoClose)
    fs.closeSync(this.fd);
  this.fd = null;
  this.emit('close');
  return true;
};

SyncWriteStream.prototype.destroySoon = SyncWriteStream.prototype.destroy;
