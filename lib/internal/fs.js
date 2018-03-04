'use strict';

const { Buffer, kMaxLength } = require('buffer');
const { Writable } = require('stream');
const errors = require('internal/errors');
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
    throw new errors.TypeError('ERR_INVALID_OPT_VALUE_ENCODING', encoding);
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
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'options',
                               ['string', 'Object'],
                               options);
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
  if (typeof m === 'string')
    return parseInt(m, 8);
  if (def)
    return modeNum(def);
  return undefined;
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

  const err = new errors.Error(
    'ERR_INVALID_ARG_VALUE', propName, path,
    'must be a string or Uint8Array without null bytes');

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

function statsFromValues(stats = statValues) {
  return new Stats(stats[0], stats[1], stats[2], stats[3], stats[4], stats[5],
                   stats[6] < 0 ? undefined : stats[6], stats[7], stats[8],
                   stats[9] < 0 ? undefined : stats[9], stats[10], stats[11],
                   stats[12], stats[13]);
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

  throw new errors.TypeError('ERR_INVALID_OPT_VALUE', 'flags', flags);
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
        const err = new errors.Error('ERR_FS_INVALID_SYMLINK_TYPE', type);
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
  throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                             name,
                             ['Date', 'Time in seconds'],
                             time);
}

function validateBuffer(buffer) {
  if (!isUint8Array(buffer)) {
    const err = new errors.TypeError('ERR_INVALID_ARG_TYPE', 'buffer',
                                     ['Buffer', 'Uint8Array']);
    Error.captureStackTrace(err, validateBuffer);
    throw err;
  }
}

function validateLen(len) {
  let err;

  if (!isInt32(len))
    err = new errors.TypeError('ERR_INVALID_ARG_TYPE', 'len', 'integer');

  if (err !== undefined) {
    Error.captureStackTrace(err, validateLen);
    throw err;
  }
}

function validateOffsetLengthRead(offset, length, bufferLength) {
  let err;

  if (offset < 0 || offset >= bufferLength) {
    err = new errors.RangeError('ERR_OUT_OF_RANGE', 'offset');
  } else if (length < 0 || offset + length > bufferLength) {
    err = new errors.RangeError('ERR_OUT_OF_RANGE', 'length');
  }

  if (err !== undefined) {
    Error.captureStackTrace(err, validateOffsetLengthRead);
    throw err;
  }
}

function validateOffsetLengthWrite(offset, length, byteLength) {
  let err;

  if (offset > byteLength) {
    err = new errors.RangeError('ERR_OUT_OF_RANGE', 'offset');
  } else if (offset + length > byteLength || offset + length > kMaxLength) {
    err = new errors.RangeError('ERR_OUT_OF_RANGE', 'length');
  }

  if (err !== undefined) {
    Error.captureStackTrace(err, validateOffsetLengthWrite);
    throw err;
  }
}

function validatePath(path, propName, assertFilename) {
  let err;

  if (propName === undefined) {
    propName = 'path';
  }

  if (typeof path === 'string') {
    err = nullCheck(path, propName, false);
    if (assertFilename && err === undefined && path[path.length - 1] === '/')
      err = new errors.Error('ERR_PATH_IS_DIRECTORY', propName, path);
  } else if (isUint8Array(path)) {
    err = nullCheck(path, propName, false);
    if (assertFilename && err === undefined && path[path.length - 1] === 47)
      err = new errors.Error('ERR_PATH_IS_DIRECTORY', propName, path);
  } else {
    err = new errors.TypeError('ERR_INVALID_ARG_TYPE', propName,
                               ['string', 'Buffer', 'URL']);
  }

  if (err !== undefined) {
    Error.captureStackTrace(err, validatePath);
    throw err;
  }
}

function validateUint32(value, propName) {
  let err;

  if (!isUint32(value))
    err = new errors.TypeError('ERR_INVALID_ARG_TYPE', propName, 'integer');

  if (err !== undefined) {
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
  statsFromValues,
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
