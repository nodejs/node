'use strict';

const Buffer = require('buffer').Buffer;
const Writable = require('stream').Writable;
const errors = require('internal/errors');
const fs = require('fs');
const util = require('util');
const pathModule = require('path');

const {
  O_APPEND,
  O_CREAT,
  O_EXCL,
  O_RDONLY,
  O_RDWR,
  O_SYNC,
  O_TRUNC,
  O_WRONLY,
  S_IFDIR,
  S_IFBLK,
  S_IFREG,
  S_IFCHR,
  S_IFMT,
  S_IFLNK,
  S_IFIFO,
  S_IFSOCK,
  UV_FS_SYMLINK_DIR,
  UV_FS_SYMLINK_JUNCTION
} = process.binding('constants').fs;

const {
  getStatValues
} = process.binding('fs');

const statValues = getStatValues();

const isWindows = process.platform === 'win32';

function assertEncoding(encoding) {
  if (encoding && !Buffer.isEncoding(encoding)) {
    throw new errors.TypeError('ERR_INVALID_OPT_VALUE_ENCODING', encoding);
  }
}

function stringToSymlinkType(type = 'file') {
  if (type != null && typeof type !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'type', 'string');
  }
  type = type || 'file';
  var flags = 0;
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
      throw new errors.TypeError('ERR_INVALID_SYMLINK_TYPE', type);
  }
  return flags;
}

function stringToFlags(flags, opt = false) {
  if (Number.isSafeInteger(flags) && flags >= 0) {
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

    case 'a+' : return O_APPEND | O_CREAT | O_RDWR;
    case 'ax+': // Fall through.
    case 'xa+': return O_APPEND | O_CREAT | O_RDWR | O_EXCL;
  }

  if (opt) {
    throw new errors.TypeError('ERR_INVALID_OPT_VALUE', 'flags', flags);
  } else {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'flags',
                               ['string', 'unsigned integer']);
  }
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

function modeNum(m, def) {
  if (typeof m === 'number')
    return m;
  if (typeof m === 'string')
    return parseInt(m, 8);
  if (def)
    return modeNum(def);
  return undefined;
}

// converts Date or number to a fractional UNIX timestamp
function toUnixTimestamp(time) {
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
  throw new errors.Error('ERR_INVALID_ARG_TYPE',
                         'time',
                         ['Date', 'time in seconds'],
                         time);
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
                               ['string', 'object'],
                               options);
  }

  if (options.encoding !== 'buffer')
    assertEncoding(options.encoding);
  return options;
}

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

function statsFromValues() {
  return new Stats(statValues[0], statValues[1], statValues[2], statValues[3],
                   statValues[4], statValues[5],
                   statValues[6] < 0 ? undefined : statValues[6], statValues[7],
                   statValues[8], statValues[9] < 0 ? undefined : statValues[9],
                   statValues[10], statValues[11], statValues[12],
                   statValues[13]);
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

module.exports = {
  assertEncoding,
  getOptions,
  modeNum,
  nullCheck,
  preprocessSymlinkDestination,
  statsFromValues,
  statsFromPrevValues,
  statValues,
  stringToFlags,
  stringToSymlinkType,
  Stats,
  SyncWriteStream,
  toUnixTimestamp,
  realpathCacheKey: Symbol('realpathCacheKey')
};
