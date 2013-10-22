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

// Maintainers, keep in mind that octal literals are not allowed
// in strict mode. Use the decimal value and add a comment with
// the octal value. Example:
//
//   var mode = 438; /* mode=0666 */

var util = require('util');
var pathModule = require('path');

var binding = process.binding('fs');
var constants = process.binding('constants');
var fs = exports;
var Stream = require('stream').Stream;
var EventEmitter = require('events').EventEmitter;

var Readable = Stream.Readable;
var Writable = Stream.Writable;

var kMinPoolSpace = 128;

var O_APPEND = constants.O_APPEND || 0;
var O_CREAT = constants.O_CREAT || 0;
var O_DIRECTORY = constants.O_DIRECTORY || 0;
var O_EXCL = constants.O_EXCL || 0;
var O_NOCTTY = constants.O_NOCTTY || 0;
var O_NOFOLLOW = constants.O_NOFOLLOW || 0;
var O_RDONLY = constants.O_RDONLY || 0;
var O_RDWR = constants.O_RDWR || 0;
var O_SYMLINK = constants.O_SYMLINK || 0;
var O_SYNC = constants.O_SYNC || 0;
var O_TRUNC = constants.O_TRUNC || 0;
var O_WRONLY = constants.O_WRONLY || 0;

var isWindows = process.platform === 'win32';

var DEBUG = process.env.NODE_DEBUG && /fs/.test(process.env.NODE_DEBUG);
var errnoException = util._errnoException;


function rethrow() {
  // Only enable in debug mode. A backtrace uses ~1000 bytes of heap space and
  // is fairly slow to generate.
  if (DEBUG) {
    var backtrace = new Error;
    return function(err) {
      if (err) {
        backtrace.stack = err.name + ': ' + err.message +
                          backtrace.stack.substr(backtrace.name.length);
        err = backtrace;
        throw err;
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
  return util.isFunction(cb) ? cb : rethrow();
}

// Ensure that callbacks run in the global context. Only use this function
// for callbacks that are passed to the binding layer, callbacks that are
// invoked from JS already run in the proper scope.
function makeCallback(cb) {
  if (!util.isFunction(cb)) {
    return rethrow();
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
    if (!callback)
      throw er;
    process.nextTick(function() {
      callback(er);
    });
    return false;
  }
  return true;
}

fs.Stats = binding.Stats;

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

fs.exists = function(path, callback) {
  if (!nullCheck(path, cb)) return;
  binding.stat(pathModule._makeLong(path), cb);
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

  if (util.isFunction(options) || !options) {
    options = { encoding: null, flag: 'r' };
  } else if (util.isString(options)) {
    options = { encoding: options, flag: 'r' };
  } else if (!util.isObject(options)) {
    throw new TypeError('Bad arguments');
  }

  var encoding = options.encoding;
  assertEncoding(encoding);

  // first, stat the file, so we know the size.
  var size;
  var buffer; // single buffer with file data
  var buffers; // list for when size is unknown
  var pos = 0;
  var fd;

  var flag = options.flag || 'r';
  fs.open(path, flag, 438 /*=0666*/, function(er, fd_) {
    if (er) return callback(er);
    fd = fd_;

    fs.fstat(fd, function(er, st) {
      if (er) return callback(er);
      size = st.size;
      if (size === 0) {
        // the kernel lies about many files.
        // Go ahead and try to read some bytes.
        buffers = [];
        return read();
      }

      buffer = new Buffer(size);
      read();
    });
  });

  function read() {
    if (size === 0) {
      buffer = new Buffer(8192);
      fs.read(fd, buffer, 0, 8192, -1, afterRead);
    } else {
      fs.read(fd, buffer, pos, size - pos, -1, afterRead);
    }
  }

  function afterRead(er, bytesRead) {
    if (er) {
      return fs.close(fd, function(er2) {
        return callback(er);
      });
    }

    if (bytesRead === 0) {
      return close();
    }

    pos += bytesRead;
    if (size !== 0) {
      if (pos === size) close();
      else read();
    } else {
      // unknown size, just read until we don't get bytes.
      buffers.push(buffer.slice(0, bytesRead));
      read();
    }
  }

  function close() {
    fs.close(fd, function(er) {
      if (size === 0) {
        // collected the data into the buffers list.
        buffer = Buffer.concat(buffers, pos);
      } else if (pos < size) {
        buffer = buffer.slice(0, pos);
      }

      if (encoding) buffer = buffer.toString(encoding);
      return callback(er, buffer);
    });
  }
};

fs.readFileSync = function(path, options) {
  if (!options) {
    options = { encoding: null, flag: 'r' };
  } else if (util.isString(options)) {
    options = { encoding: options, flag: 'r' };
  } else if (!util.isObject(options)) {
    throw new TypeError('Bad arguments');
  }

  var encoding = options.encoding;
  assertEncoding(encoding);

  var flag = options.flag || 'r';
  var fd = fs.openSync(path, flag, 438 /*=0666*/);

  var size;
  var threw = true;
  try {
    size = fs.fstatSync(fd).size;
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
    buffer = new Buffer(size);
  }

  var done = false;
  while (!done) {
    var threw = true;
    try {
      if (size !== 0) {
        var bytesRead = fs.readSync(fd, buffer, pos, size - pos);
      } else {
        // the kernel lies about many files.
        // Go ahead and try to read some bytes.
        buffer = new Buffer(8192);
        var bytesRead = fs.readSync(fd, buffer, 0, 8192);
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
  if (!util.isString(flag)) {
    return flag;
  }

  switch (flag) {
    case 'r' : return O_RDONLY;
    case 'rs' : return O_RDONLY | O_SYNC;
    case 'r+' : return O_RDWR;
    case 'rs+' : return O_RDWR | O_SYNC;

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
  binding.close(fd, makeCallback(callback));
};

fs.closeSync = function(fd) {
  return binding.close(fd);
};

function modeNum(m, def) {
  if (util.isNumber(m))
    return m;
  if (util.isString(m))
    return parseInt(m, 8);
  if (def)
    return modeNum(def);
  return undefined;
}

fs.open = function(path, flags, mode, callback) {
  callback = makeCallback(arguments[arguments.length - 1]);
  mode = modeNum(mode, 438 /*=0666*/);

  if (!nullCheck(path, callback)) return;
  binding.open(pathModule._makeLong(path),
               stringToFlags(flags),
               mode,
               callback);
};

fs.openSync = function(path, flags, mode) {
  mode = modeNum(mode, 438 /*=0666*/);
  nullCheck(path);
  return binding.open(pathModule._makeLong(path), stringToFlags(flags), mode);
};

fs.read = function(fd, buffer, offset, length, position, callback) {
  if (!util.isBuffer(buffer)) {
    // legacy string interface (fd, length, position, encoding, callback)
    var cb = arguments[4],
        encoding = arguments[3];

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

  binding.read(fd, buffer, offset, length, position, wrapper);
};

fs.readSync = function(fd, buffer, offset, length, position) {
  var legacy = false;
  if (!util.isBuffer(buffer)) {
    // legacy string interface (fd, length, position, encoding, callback)
    legacy = true;
    var encoding = arguments[3];

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
  if (util.isBuffer(buffer)) {
    // if no position is passed then assume null
    if (util.isFunction(position)) {
      callback = position;
      position = null;
    }
    callback = maybeCallback(callback);
    var wrapper = function(err, written) {
      // Retain a reference to buffer so that it can't be GC'ed too soon.
      callback(err, written || 0, buffer);
    };
    return binding.writeBuffer(fd, buffer, offset, length, position, wrapper);
  }

  if (util.isString(buffer))
    buffer += '';
  if (!util.isFunction(position)) {
    if (util.isFunction(offset)) {
      position = offset;
      offset = null;
    } else {
      position = length;
    }
    length = 'utf8';
  }
  callback = maybeCallback(position);
  position = function(err, written) {
    // retain reference to string in case it's external
    callback(err, written || 0, buffer);
  };
  return binding.writeString(fd, buffer, offset, length, position);
};

// usage:
//  fs.writeSync(fd, buffer, offset, length[, position]);
// OR
//  fs.writeSync(fd, string[, position[, encoding]]);
fs.writeSync = function(fd, buffer, offset, length, position) {
  if (util.isBuffer(buffer)) {
    if (util.isUndefined(position))
      position = null;
    return binding.writeBuffer(fd, buffer, offset, length, position);
  }
  if (!util.isString(buffer))
    buffer += '';
  if (util.isUndefined(offset))
    offset = null;
  return binding.writeString(fd, buffer, offset, length, position);
};

fs.rename = function(oldPath, newPath, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(oldPath, callback)) return;
  if (!nullCheck(newPath, callback)) return;
  binding.rename(pathModule._makeLong(oldPath),
                 pathModule._makeLong(newPath),
                 callback);
};

fs.renameSync = function(oldPath, newPath) {
  nullCheck(oldPath);
  nullCheck(newPath);
  return binding.rename(pathModule._makeLong(oldPath),
                        pathModule._makeLong(newPath));
};

fs.truncate = function(path, len, callback) {
  if (util.isNumber(path)) {
    // legacy
    return fs.ftruncate(path, len, callback);
  }
  if (util.isFunction(len)) {
    callback = len;
    len = 0;
  } else if (util.isUndefined(len)) {
    len = 0;
  }
  callback = maybeCallback(callback);
  fs.open(path, 'r+', function(er, fd) {
    if (er) return callback(er);
    binding.ftruncate(fd, len, function(er) {
      fs.close(fd, function(er2) {
        callback(er || er2);
      });
    });
  });
};

fs.truncateSync = function(path, len) {
  if (util.isNumber(path)) {
    // legacy
    return fs.ftruncateSync(path, len);
  }
  if (util.isUndefined(len)) {
    len = 0;
  }
  // allow error to be thrown, but still close fd.
  var fd = fs.openSync(path, 'r+');
  try {
    var ret = fs.ftruncateSync(fd, len);
  } finally {
    fs.closeSync(fd);
  }
  return ret;
};

fs.ftruncate = function(fd, len, callback) {
  if (util.isFunction(len)) {
    callback = len;
    len = 0;
  } else if (util.isUndefined(len)) {
    len = 0;
  }
  binding.ftruncate(fd, len, makeCallback(callback));
};

fs.ftruncateSync = function(fd, len) {
  if (util.isUndefined(len)) {
    len = 0;
  }
  return binding.ftruncate(fd, len);
};

fs.rmdir = function(path, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  binding.rmdir(pathModule._makeLong(path), callback);
};

fs.rmdirSync = function(path) {
  nullCheck(path);
  return binding.rmdir(pathModule._makeLong(path));
};

fs.fdatasync = function(fd, callback) {
  binding.fdatasync(fd, makeCallback(callback));
};

fs.fdatasyncSync = function(fd) {
  return binding.fdatasync(fd);
};

fs.fsync = function(fd, callback) {
  binding.fsync(fd, makeCallback(callback));
};

fs.fsyncSync = function(fd) {
  return binding.fsync(fd);
};

fs.mkdir = function(path, mode, callback) {
  if (util.isFunction(mode)) callback = mode;
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  binding.mkdir(pathModule._makeLong(path),
                modeNum(mode, 511 /*=0777*/),
                callback);
};

fs.mkdirSync = function(path, mode) {
  nullCheck(path);
  return binding.mkdir(pathModule._makeLong(path),
                       modeNum(mode, 511 /*=0777*/));
};

fs.readdir = function(path, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  binding.readdir(pathModule._makeLong(path), callback);
};

fs.readdirSync = function(path) {
  nullCheck(path);
  return binding.readdir(pathModule._makeLong(path));
};

fs.fstat = function(fd, callback) {
  binding.fstat(fd, makeCallback(callback));
};

fs.lstat = function(path, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  binding.lstat(pathModule._makeLong(path), callback);
};

fs.stat = function(path, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  binding.stat(pathModule._makeLong(path), callback);
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
  binding.readlink(pathModule._makeLong(path), callback);
};

fs.readlinkSync = function(path) {
  nullCheck(path);
  return binding.readlink(pathModule._makeLong(path));
};

function preprocessSymlinkDestination(path, type) {
  if (!isWindows) {
    // No preprocessing is needed on Unix.
    return path;
  } else if (type === 'junction') {
    // Junctions paths need to be absolute and \\?\-prefixed.
    return pathModule._makeLong(path);
  } else {
    // Windows symlinks don't tolerate forward slashes.
    return ('' + path).replace(/\//g, '\\');
  }
}

fs.symlink = function(destination, path, type_, callback) {
  var type = (util.isString(type_) ? type_ : null);
  var callback = makeCallback(arguments[arguments.length - 1]);

  if (!nullCheck(destination, callback)) return;
  if (!nullCheck(path, callback)) return;

  binding.symlink(preprocessSymlinkDestination(destination, type),
                  pathModule._makeLong(path),
                  type,
                  callback);
};

fs.symlinkSync = function(destination, path, type) {
  type = (util.isString(type) ? type : null);

  nullCheck(destination);
  nullCheck(path);

  return binding.symlink(preprocessSymlinkDestination(destination, type),
                         pathModule._makeLong(path),
                         type);
};

fs.link = function(srcpath, dstpath, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(srcpath, callback)) return;
  if (!nullCheck(dstpath, callback)) return;

  binding.link(pathModule._makeLong(srcpath),
               pathModule._makeLong(dstpath),
               callback);
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
  binding.unlink(pathModule._makeLong(path), callback);
};

fs.unlinkSync = function(path) {
  nullCheck(path);
  return binding.unlink(pathModule._makeLong(path));
};

fs.fchmod = function(fd, mode, callback) {
  binding.fchmod(fd, modeNum(mode), makeCallback(callback));
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
    var err, err2;
    try {
      var ret = fs.fchmodSync(fd, mode);
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
  binding.chmod(pathModule._makeLong(path),
                modeNum(mode),
                callback);
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
  binding.fchown(fd, uid, gid, makeCallback(callback));
};

fs.fchownSync = function(fd, uid, gid) {
  return binding.fchown(fd, uid, gid);
};

fs.chown = function(path, uid, gid, callback) {
  callback = makeCallback(callback);
  if (!nullCheck(path, callback)) return;
  binding.chown(pathModule._makeLong(path), uid, gid, callback);
};

fs.chownSync = function(path, uid, gid) {
  nullCheck(path);
  return binding.chown(pathModule._makeLong(path), uid, gid);
};

// converts Date or number to a fractional UNIX timestamp
function toUnixTimestamp(time) {
  if (util.isNumber(time)) {
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
  binding.utimes(pathModule._makeLong(path),
                 toUnixTimestamp(atime),
                 toUnixTimestamp(mtime),
                 callback);
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
  binding.futimes(fd, atime, mtime, makeCallback(callback));
};

fs.futimesSync = function(fd, atime, mtime) {
  atime = toUnixTimestamp(atime);
  mtime = toUnixTimestamp(mtime);
  binding.futimes(fd, atime, mtime);
};

function writeAll(fd, buffer, offset, length, position, callback) {
  callback = maybeCallback(arguments[arguments.length - 1]);

  // write(fd, buffer, offset, length, position, callback)
  fs.write(fd, buffer, offset, length, position, function(writeErr, written) {
    if (writeErr) {
      fs.close(fd, function() {
        if (callback) callback(writeErr);
      });
    } else {
      if (written === length) {
        fs.close(fd, callback);
      } else {
        offset += written;
        length -= written;
        position += written;
        writeAll(fd, buffer, offset, length, position, callback);
      }
    }
  });
}

fs.writeFile = function(path, data, options, callback) {
  var callback = maybeCallback(arguments[arguments.length - 1]);

  if (util.isFunction(options) || !options) {
    options = { encoding: 'utf8', mode: 438 /*=0666*/, flag: 'w' };
  } else if (util.isString(options)) {
    options = { encoding: options, mode: 438, flag: 'w' };
  } else if (!util.isObject(options)) {
    throw new TypeError('Bad arguments');
  }

  assertEncoding(options.encoding);

  var flag = options.flag || 'w';
  fs.open(path, options.flag || 'w', options.mode, function(openErr, fd) {
    if (openErr) {
      if (callback) callback(openErr);
    } else {
      var buffer = util.isBuffer(data) ? data : new Buffer('' + data,
          options.encoding || 'utf8');
      var position = /a/.test(flag) ? null : 0;
      writeAll(fd, buffer, 0, buffer.length, position, callback);
    }
  });
};

fs.writeFileSync = function(path, data, options) {
  if (!options) {
    options = { encoding: 'utf8', mode: 438 /*=0666*/, flag: 'w' };
  } else if (util.isString(options)) {
    options = { encoding: options, mode: 438, flag: 'w' };
  } else if (!util.isObject(options)) {
    throw new TypeError('Bad arguments');
  }

  assertEncoding(options.encoding);

  var flag = options.flag || 'w';
  var fd = fs.openSync(path, flag, options.mode);
  if (!util.isBuffer(data)) {
    data = new Buffer('' + data, options.encoding || 'utf8');
  }
  var written = 0;
  var length = data.length;
  var position = /a/.test(flag) ? null : 0;
  try {
    while (written < length) {
      written += fs.writeSync(fd, data, written, length - written, position);
      position += written;
    }
  } finally {
    fs.closeSync(fd);
  }
};

fs.appendFile = function(path, data, options, callback_) {
  var callback = maybeCallback(arguments[arguments.length - 1]);

  if (util.isFunction(options) || !options) {
    options = { encoding: 'utf8', mode: 438 /*=0666*/, flag: 'a' };
  } else if (util.isString(options)) {
    options = { encoding: options, mode: 438, flag: 'a' };
  } else if (!util.isObject(options)) {
    throw new TypeError('Bad arguments');
  }

  if (!options.flag)
    options = util._extend({ flag: 'a' }, options);
  fs.writeFile(path, data, options, callback);
};

fs.appendFileSync = function(path, data, options) {
  if (!options) {
    options = { encoding: 'utf8', mode: 438 /*=0666*/, flag: 'a' };
  } else if (util.isString(options)) {
    options = { encoding: options, mode: 438, flag: 'a' };
  } else if (!util.isObject(options)) {
    throw new TypeError('Bad arguments');
  }
  if (!options.flag)
    options = util._extend({ flag: 'a' }, options);

  fs.writeFileSync(path, data, options);
};

function FSWatcher() {
  EventEmitter.call(this);

  var self = this;
  var FSEvent = process.binding('fs_event_wrap').FSEvent;
  this._handle = new FSEvent();
  this._handle.owner = this;

  this._handle.onchange = function(status, event, filename) {
    if (status < 0) {
      self._handle.close();
      self.emit('error', errnoException(status, 'watch'));
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
    throw errnoException(err, 'watch');
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

  if (util.isObject(arguments[1])) {
    options = arguments[1];
    listener = arguments[2];
  } else {
    options = {};
    listener = arguments[1];
  }

  if (util.isUndefined(options.persistent)) options.persistent = true;
  if (util.isUndefined(options.recursive)) options.recursive = false;

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


var statWatchers = {};
function inStatWatchers(filename) {
  return Object.prototype.hasOwnProperty.call(statWatchers, filename) &&
      statWatchers[filename];
}


fs.watchFile = function(filename) {
  nullCheck(filename);
  var stat;
  var listener;

  var options = {
    // Poll interval in milliseconds. 5007 is what libev used to use. It's
    // a little on the slow side but let's stick with it for now to keep
    // behavioral changes to a minimum.
    interval: 5007,
    persistent: true
  };

  if (util.isObject(arguments[1])) {
    options = util._extend(options, arguments[1]);
    listener = arguments[2];
  } else {
    listener = arguments[1];
  }

  if (!listener) {
    throw new Error('watchFile requires a listener function');
  }

  if (inStatWatchers(filename)) {
    stat = statWatchers[filename];
  } else {
    stat = statWatchers[filename] = new StatWatcher();
    stat.start(filename, options.persistent, options.interval);
  }
  stat.addListener('change', listener);
  return stat;
};

fs.unwatchFile = function(filename, listener) {
  nullCheck(filename);
  if (!inStatWatchers(filename)) return;

  var stat = statWatchers[filename];

  if (util.isFunction(listener)) {
    stat.removeListener('change', listener);
  } else {
    stat.removeAllListeners('change');
  }

  if (EventEmitter.listenerCount(stat, 'change') === 0) {
    stat.stop();
    statWatchers[filename] = undefined;
  }
};

// Regexp that finds the next partion of a (partial) path
// result is [base_with_slash, base], e.g. ['somedir/', 'somedir']
if (isWindows) {
  var nextPartRe = /(.*?)(?:[\/\\]+|$)/g;
} else {
  var nextPartRe = /(.*?)(?:[\/]+|$)/g;
}

// Regex to find the device root, including trailing slash. E.g. 'c:\\'.
if (isWindows) {
  var splitRootRe = /^(?:[a-zA-Z]:|[\\\/]{2}[^\\\/]+[\\\/][^\\\/]+)?[\\\/]*/;
} else {
  var splitRootRe = /^[\/]*/;
}

fs.realpathSync = function realpathSync(p, cache) {
  // make p is absolute
  p = pathModule.resolve(p);

  if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
    return cache[p];
  }

  var original = p,
      seenLinks = {},
      knownHard = {};

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
      if (util.isNull(linkTarget)) {
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
  if (!util.isFunction(cb)) {
    cb = maybeCallback(cache);
    cache = null;
  }

  // make p is absolute
  p = pathModule.resolve(p);

  if (cache && Object.prototype.hasOwnProperty.call(cache, p)) {
    return process.nextTick(cb.bind(null, null, cache[p]));
  }

  var original = p,
      seenLinks = {},
      knownHard = {};

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

  // a little bit bigger buffer and water marks by default
  options = util._extend({
    highWaterMark: 64 * 1024
  }, options || {});

  Readable.call(this, options);

  this.path = path;
  this.fd = options.hasOwnProperty('fd') ? options.fd : null;
  this.flags = options.hasOwnProperty('flags') ? options.flags : 'r';
  this.mode = options.hasOwnProperty('mode') ? options.mode : 438; /*=0666*/

  this.start = options.hasOwnProperty('start') ? options.start : undefined;
  this.end = options.hasOwnProperty('end') ? options.end : undefined;
  this.autoClose = options.hasOwnProperty('autoClose') ?
      options.autoClose : true;
  this.pos = undefined;

  if (!util.isUndefined(this.start)) {
    if (!util.isNumber(this.start)) {
      throw TypeError('start must be a Number');
    }
    if (util.isUndefined(this.end)) {
      this.end = Infinity;
    } else if (!util.isNumber(this.end)) {
      throw TypeError('end must be a Number');
    }

    if (this.start > this.end) {
      throw new Error('start must be <= end');
    }

    this.pos = this.start;
  }

  if (!util.isNumber(this.fd))
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
  if (!util.isNumber(this.fd))
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

  if (!util.isUndefined(this.pos))
    toRead = Math.min(this.end - this.pos + 1, toRead);

  // already read everything we were supposed to read!
  // treat as EOF.
  if (toRead <= 0)
    return this.push(null);

  // the actual read.
  var self = this;
  fs.read(this.fd, pool, pool.used, toRead, this.pos, onread);

  // move the pool positions, and internal position for reading.
  if (!util.isUndefined(this.pos))
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

  if (util.isNumber(this.fd))
    this.close();
};


ReadStream.prototype.close = function(cb) {
  var self = this;
  if (cb)
    this.once('close', cb);
  if (this.closed || !util.isNumber(this.fd)) {
    if (!util.isNumber(this.fd)) {
      this.once('open', close);
      return;
    }
    return process.nextTick(this.emit.bind(this, 'close'));
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

  options = options || {};

  Writable.call(this, options);

  this.path = path;
  this.fd = null;

  this.fd = options.hasOwnProperty('fd') ? options.fd : null;
  this.flags = options.hasOwnProperty('flags') ? options.flags : 'w';
  this.mode = options.hasOwnProperty('mode') ? options.mode : 438; /*=0666*/

  this.start = options.hasOwnProperty('start') ? options.start : undefined;
  this.pos = undefined;
  this.bytesWritten = 0;

  if (!util.isUndefined(this.start)) {
    if (!util.isNumber(this.start)) {
      throw TypeError('start must be a Number');
    }
    if (this.start < 0) {
      throw new Error('start must be >= zero');
    }

    this.pos = this.start;
  }

  if (!util.isNumber(this.fd))
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
  if (!util.isBuffer(data))
    return this.emit('error', new Error('Invalid data'));

  if (!util.isNumber(this.fd))
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

  if (!util.isUndefined(this.pos))
    this.pos += data.length;
};


WriteStream.prototype.destroy = ReadStream.prototype.destroy;
WriteStream.prototype.close = ReadStream.prototype.close;

// There is no shutdown() for files.
WriteStream.prototype.destroySoon = WriteStream.prototype.end;


// SyncWriteStream is internal. DO NOT USE.
// Temporary hack for process.stdout and process.stderr when piped to files.
function SyncWriteStream(fd) {
  Stream.call(this);

  this.fd = fd;
  this.writable = true;
  this.readable = false;
}

util.inherits(SyncWriteStream, Stream);


// Export
fs.SyncWriteStream = SyncWriteStream;


SyncWriteStream.prototype.write = function(data, arg1, arg2) {
  var encoding, cb;

  // parse arguments
  if (arg1) {
    if (util.isString(arg1)) {
      encoding = arg1;
      cb = arg2;
    } else if (util.isFunction(arg1)) {
      cb = arg1;
    } else {
      throw new Error('bad arg');
    }
  }
  assertEncoding(encoding);

  // Change strings to buffers. SLOW
  if (util.isString(data)) {
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
  fs.closeSync(this.fd);
  this.fd = null;
  this.emit('close');
  return true;
};

SyncWriteStream.prototype.destroySoon = SyncWriteStream.prototype.destroy;
