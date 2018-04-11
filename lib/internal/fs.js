'use strict';

const { Buffer, kMaxLength } = require('buffer');
const { Writable } = require('stream');
const {
  ERR_FS_INVALID_SYMLINK_TYPE,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_OPT_VALUE,
  ERR_INVALID_OPT_VALUE_ENCODING,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { isUint8Array } = require('internal/util/types');
const fs = require('fs');
const pathModule = require('path');
const util = require('util');

const {
  O_APPEND,
  O_CREAT,
  O_EXCL,
  O_RDONLY,
  O_RDWR,
  O_SYNC,
  O_TRUNC,
  O_WRONLY,
  S_IFBLK,
  S_IFCHR,
  S_IFDIR,
  S_IFIFO,
  S_IFLNK,
  S_IFMT,
  S_IFREG,
  S_IFSOCK,
  UV_FS_SYMLINK_DIR,
  UV_FS_SYMLINK_JUNCTION
} = process.binding('constants').fs;
const { statValues } = process.binding('fs');

const isWindows = process.platform === 'win32';

function assertEncoding(encoding) {
  if (encoding && !Buffer.isEncoding(encoding)) {
    throw new ERR_INVALID_OPT_VALUE_ENCODING(encoding);
  }
}

function copyObject(source) {
  var target = {};
  for (var key in source)
    target[key] = source[key];
  return target;
}

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
    throw new ERR_INVALID_ARG_TYPE('options', ['string', 'Object'], options);
  }

  if (options.encoding !== 'buffer')
    assertEncoding(options.encoding);
  return options;
}

function isInt32(n) { return n === (n | 0); }
function isUint32(n) { return n === (n >>> 0); }

function modeNum(m, def) {
  if (typeof m === 'number')
    return m;
  if (typeof m === 'string') {
    const parsed = parseInt(m, 8);
    if (Number.isNaN(parsed))
      return m;
    return parsed;
  }
  // TODO(BridgeAR): Only return `def` in case `m == null`
  if (def !== undefined)
    return def;
  return m;
}

// Check if the path contains null types if it is a string nor Uint8Array,
// otherwise return silently.
function nullCheck(path, propName, throwError = true) {
  const pathIsString = typeof path === 'string';
  const pathIsUint8Array = isUint8Array(path);

  // We can only perform meaningful checks on strings and Uint8Arrays.
  if (!pathIsString && !pathIsUint8Array) {
    return;
  }

  if (pathIsString && path.indexOf('\u0000') === -1) {
    return;
  } else if (pathIsUint8Array && path.indexOf(0) === -1) {
    return;
  }

  const err = new ERR_INVALID_ARG_VALUE(
    propName,
    path,
    'must be a string or Uint8Array without null bytes'
  );

  if (throwError) {
    Error.captureStackTrace(err, nullCheck);
    throw err;
  }
  return err;
}

function preprocessSymlinkDestination(path, type, linkPath) {
  if (!isWindows) {
    // No preprocessing is needed on Unix.
    return path;
  } else if (type === 'junction') {
    // Junctions paths need to be absolute and \\?\-prefixed.
    // A relative target is relative to the link's parent directory.
    path = pathModule.resolve(linkPath, '..', path);
    return pathModule.toNamespacedPath(path);
  } else {
    // Windows symlinks don't tolerate forward slashes.
    return ('' + path).replace(/\//g, '\\');
  }
}

function dateFromNumeric(num) {
  return new Date(num + 0.5);
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
  this.atime = dateFromNumeric(atim_msec);
  this.mtime = dateFromNumeric(mtim_msec);
  this.ctime = dateFromNumeric(ctim_msec);
  this.birthtime = dateFromNumeric(birthtim_msec);
}

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

function getStatsFromBinding(stats, offset = 0) {
  return new Stats(stats[0 + offset], stats[1 + offset], stats[2 + offset],
                   stats[3 + offset], stats[4 + offset], stats[5 + offset],
                   stats[6 + offset] < 0 ? undefined : stats[6 + offset],
                   stats[7 + offset], stats[8 + offset],
                   stats[9 + offset] < 0 ? undefined : stats[9 + offset],
                   stats[10 + offset], stats[11 + offset],
                   stats[12 + offset], stats[13 + offset]);
}

function getStatsFromGlobalBinding(offset = 0) {
  return getStatsFromBinding(statValues, offset);
}

function stringToFlags(flags) {
  if (typeof flags === 'number') {
    return flags;
  }

  switch (flags) {
    case 'r' : return O_RDONLY;
    case 'rs' : // Fall through.
    case 'sr' : return O_RDONLY | O_SYNC;
    case 'r+' : return O_RDWR;
    case 'rs+' : // Fall through.
    case 'sr+' : return O_RDWR | O_SYNC;

    case 'w' : return O_TRUNC | O_CREAT | O_WRONLY;
    case 'wx' : // Fall through.
    case 'xw' : return O_TRUNC | O_CREAT | O_WRONLY | O_EXCL;

    case 'w+' : return O_TRUNC | O_CREAT | O_RDWR;
    case 'wx+': // Fall through.
    case 'xw+': return O_TRUNC | O_CREAT | O_RDWR | O_EXCL;

    case 'a' : return O_APPEND | O_CREAT | O_WRONLY;
    case 'ax' : // Fall through.
    case 'xa' : return O_APPEND | O_CREAT | O_WRONLY | O_EXCL;
    case 'as' : // Fall through.
    case 'sa' : return O_APPEND | O_CREAT | O_WRONLY | O_SYNC;

    case 'a+' : return O_APPEND | O_CREAT | O_RDWR;
    case 'ax+': // Fall through.
    case 'xa+': return O_APPEND | O_CREAT | O_RDWR | O_EXCL;
    case 'as+': // Fall through.
    case 'sa+': return O_APPEND | O_CREAT | O_RDWR | O_SYNC;
  }

  throw new ERR_INVALID_OPT_VALUE('flags', flags);
}

function stringToSymlinkType(type) {
  let flags = 0;
  if (typeof type === 'string') {
    switch (type) {
      case 'dir':
        flags |= UV_FS_SYMLINK_DIR;
        break;
      case 'junction':
        flags |= UV_FS_SYMLINK_JUNCTION;
        break;
      case 'file':
        break;
      default:
        const err = new ERR_FS_INVALID_SYMLINK_TYPE(type);
        Error.captureStackTrace(err, stringToSymlinkType);
        throw err;
    }
  }
  return flags;
}

// Temporary hack for process.stdout and process.stderr when piped to files.
function SyncWriteStream(fd, options) {
  Writable.call(this);

  options = options || {};

  this.fd = fd;
  this.readable = false;
  this.autoClose = options.autoClose === undefined ? true : options.autoClose;

  this.on('end', () => this._destroy());
}

util.inherits(SyncWriteStream, Writable);

SyncWriteStream.prototype._write = function(chunk, encoding, cb) {
  fs.writeSync(this.fd, chunk, 0, chunk.length);
  cb();
  return true;
};

SyncWriteStream.prototype._destroy = function() {
  if (this.fd === null) // already destroy()ed
    return;

  if (this.autoClose)
    fs.closeSync(this.fd);

  this.fd = null;
  return true;
};

SyncWriteStream.prototype.destroySoon =
SyncWriteStream.prototype.destroy = function() {
  this._destroy();
  this.emit('close');
  return true;
};

// converts Date or number to a fractional UNIX timestamp
function toUnixTimestamp(time, name = 'time') {
  // eslint-disable-next-line eqeqeq
  if (typeof time === 'string' && +time == time) {
    return +time;
  }
  if (Number.isFinite(time)) {
    if (time < 0) {
      return Date.now() / 1000;
    }
    return time;
  }
  if (util.isDate(time)) {
    // convert to 123.456 UNIX timestamp
    return time.getTime() / 1000;
  }
  throw new ERR_INVALID_ARG_TYPE(name, ['Date', 'Time in seconds'], time);
}

function validateBuffer(buffer) {
  if (!isUint8Array(buffer)) {
    const err = new ERR_INVALID_ARG_TYPE('buffer',
                                         ['Buffer', 'Uint8Array'], buffer);
    Error.captureStackTrace(err, validateBuffer);
    throw err;
  }
}

function validateLen(len) {
  let err;

  if (!isInt32(len)) {
    if (typeof len !== 'number') {
      err = new ERR_INVALID_ARG_TYPE('len', 'number', len);
    } else if (!Number.isInteger(len)) {
      err = new ERR_OUT_OF_RANGE('len', 'an integer', len);
    } else {
      // 2 ** 31 === 2147483648
      err = new ERR_OUT_OF_RANGE('len', '> -2147483649 && < 2147483648', len);
    }
  }

  if (err !== undefined) {
    Error.captureStackTrace(err, validateLen);
    throw err;
  }
}

function validateOffsetLengthRead(offset, length, bufferLength) {
  let err;

  if (offset < 0 || offset >= bufferLength) {
    err = new ERR_OUT_OF_RANGE('offset');
  } else if (length < 0 || offset + length > bufferLength) {
    err = new ERR_OUT_OF_RANGE('length');
  }

  if (err !== undefined) {
    Error.captureStackTrace(err, validateOffsetLengthRead);
    throw err;
  }
}

function validateOffsetLengthWrite(offset, length, byteLength) {
  let err;

  if (offset > byteLength) {
    err = new ERR_OUT_OF_RANGE('offset');
  } else if (offset + length > byteLength || offset + length > kMaxLength) {
    err = new ERR_OUT_OF_RANGE('length');
  }

  if (err !== undefined) {
    Error.captureStackTrace(err, validateOffsetLengthWrite);
    throw err;
  }
}

function validatePath(path, propName = 'path') {
  let err;

  if (typeof path !== 'string' && !isUint8Array(path)) {
    err = new ERR_INVALID_ARG_TYPE(propName, ['string', 'Buffer', 'URL'], path);
  } else {
    err = nullCheck(path, propName, false);
  }

  if (err !== undefined) {
    Error.captureStackTrace(err, validatePath);
    throw err;
  }
}

function validateUint32(value, propName) {
  if (!isUint32(value)) {
    let err;
    if (typeof value !== 'number') {
      err = new ERR_INVALID_ARG_TYPE(propName, 'number', value);
    } else if (!Number.isInteger(value)) {
      err = new ERR_OUT_OF_RANGE(propName, 'an integer', value);
    } else {
      // 2 ** 32 === 4294967296
      err = new ERR_OUT_OF_RANGE(propName, '>= 0 && < 4294967296', value);
    }
    Error.captureStackTrace(err, validateUint32);
    throw err;
  }
}

module.exports = {
  assertEncoding,
  copyObject,
  getOptions,
  isInt32,
  isUint32,
  modeNum,
  nullCheck,
  preprocessSymlinkDestination,
  realpathCacheKey: Symbol('realpathCacheKey'),
  getStatsFromBinding,
  getStatsFromGlobalBinding,
  stringToFlags,
  stringToSymlinkType,
  Stats,
  SyncWriteStream,
  toUnixTimestamp,
  validateBuffer,
  validateLen,
  validateOffsetLengthRead,
  validateOffsetLengthWrite,
  validatePath,
  validateUint32
};
