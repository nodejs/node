'use strict';

const { Buffer, kMaxLength } = require('buffer');
const {
  ERR_FS_INVALID_SYMLINK_TYPE,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_OPT_VALUE,
  ERR_INVALID_OPT_VALUE_ENCODING,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { isUint8Array, isArrayBufferView } = require('internal/util/types');
const { once } = require('internal/util');
const pathModule = require('path');
const util = require('util');
const kType = Symbol('type');
const kStats = Symbol('stats');

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
  UV_FS_SYMLINK_JUNCTION,
  UV_DIRENT_UNKNOWN,
  UV_DIRENT_FILE,
  UV_DIRENT_DIR,
  UV_DIRENT_LINK,
  UV_DIRENT_FIFO,
  UV_DIRENT_SOCKET,
  UV_DIRENT_CHAR,
  UV_DIRENT_BLOCK
} = internalBinding('constants').fs;

const isWindows = process.platform === 'win32';

let fs;
function lazyLoadFs() {
  if (!fs) {
    fs = require('fs');
  }
  return fs;
}

function assertEncoding(encoding) {
  if (encoding && !Buffer.isEncoding(encoding)) {
    throw new ERR_INVALID_OPT_VALUE_ENCODING(encoding);
  }
}

class Dirent {
  constructor(name, type) {
    this.name = name;
    this[kType] = type;
  }

  isDirectory() {
    return this[kType] === UV_DIRENT_DIR;
  }

  isFile() {
    return this[kType] === UV_DIRENT_FILE;
  }

  isBlockDevice() {
    return this[kType] === UV_DIRENT_BLOCK;
  }

  isCharacterDevice() {
    return this[kType] === UV_DIRENT_CHAR;
  }

  isSymbolicLink() {
    return this[kType] === UV_DIRENT_LINK;
  }

  isFIFO() {
    return this[kType] === UV_DIRENT_FIFO;
  }

  isSocket() {
    return this[kType] === UV_DIRENT_SOCKET;
  }
}

class DirentFromStats extends Dirent {
  constructor(name, stats) {
    super(name, null);
    this[kStats] = stats;
  }
}

for (const name of Reflect.ownKeys(Dirent.prototype)) {
  if (name === 'constructor') {
    continue;
  }
  DirentFromStats.prototype[name] = function() {
    return this[kStats][name]();
  };
}

function copyObject(source) {
  const target = {};
  for (const key in source)
    target[key] = source[key];
  return target;
}

function getDirents(path, [names, types], callback) {
  let i;
  if (typeof callback === 'function') {
    const len = names.length;
    let toFinish = 0;
    callback = once(callback);
    for (i = 0; i < len; i++) {
      const type = types[i];
      if (type === UV_DIRENT_UNKNOWN) {
        const name = names[i];
        const idx = i;
        toFinish++;
        lazyLoadFs().lstat(pathModule.join(path, name), (err, stats) => {
          if (err) {
            callback(err);
            return;
          }
          names[idx] = new DirentFromStats(name, stats);
          if (--toFinish === 0) {
            callback(null, names);
          }
        });
      } else {
        names[i] = new Dirent(names[i], types[i]);
      }
    }
    if (toFinish === 0) {
      callback(null, names);
    }
  } else {
    const len = names.length;
    for (i = 0; i < len; i++) {
      const type = types[i];
      if (type === UV_DIRENT_UNKNOWN) {
        const name = names[i];
        const stats = lazyLoadFs().lstatSync(pathModule.join(path, name));
        names[i] = new DirentFromStats(name, stats);
      } else {
        names[i] = new Dirent(names[i], types[i]);
      }
    }
    return names;
  }
}

function getOptions(options, defaultOptions) {
  if (options === null || options === undefined ||
      typeof options === 'function') {
    return defaultOptions;
  }

  if (typeof options === 'string') {
    defaultOptions = { ...defaultOptions };
    defaultOptions.encoding = options;
    options = defaultOptions;
  } else if (typeof options !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('options', ['string', 'Object'], options);
  }

  if (options.encoding !== 'buffer')
    assertEncoding(options.encoding);
  return options;
}

// Check if the path contains null types if it is a string nor Uint8Array,
// otherwise return silently.
function nullCheck(path, propName, throwError = true) {
  const pathIsString = typeof path === 'string';
  const pathIsUint8Array = isUint8Array(path);

  // We can only perform meaningful checks on strings and Uint8Arrays.
  if (!pathIsString && !pathIsUint8Array ||
      pathIsString && path.indexOf('\u0000') === -1 ||
      pathIsUint8Array && path.indexOf(0) === -1) {
    return;
  }

  // Reducing the limit improves the performance significantly. We do not loose
  // the stack frames due to the `captureStackTrace()` function that is called
  // later.
  const tmpLimit = Error.stackTraceLimit;
  if (throwError) {
    Error.stackTraceLimit = 0;
  }
  const err = new ERR_INVALID_ARG_VALUE(
    propName,
    path,
    'must be a string or Uint8Array without null bytes'
  );

  if (throwError) {
    Error.stackTraceLimit = tmpLimit;
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
  return new Date(Number(num) + 0.5);
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
  if (isWindows && (property === S_IFIFO || property === S_IFBLK ||
    property === S_IFSOCK)) {
    return false;  // Some types are not available on Windows
  }
  if (typeof this.mode === 'bigint') {  // eslint-disable-line valid-typeof
    return (this.mode & BigInt(S_IFMT)) === BigInt(property);
  }
  return (this.mode & S_IFMT) === property;
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
                   isWindows ? undefined : stats[6 + offset],  // blksize
                   stats[7 + offset], stats[8 + offset],
                   isWindows ? undefined : stats[9 + offset],  // blocks
                   stats[10 + offset], stats[11 + offset],
                   stats[12 + offset], stats[13 + offset]);
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
  if (!isArrayBufferView(buffer)) {
    const err = new ERR_INVALID_ARG_TYPE('buffer',
                                         ['Buffer', 'TypedArray', 'DataView'],
                                         buffer);
    Error.captureStackTrace(err, validateBuffer);
    throw err;
  }
}

function validateOffsetLengthRead(offset, length, bufferLength) {
  let err;

  if (offset < 0 || offset >= bufferLength) {
    err = new ERR_OUT_OF_RANGE('offset',
                               `>= 0 && <= ${bufferLength}`, offset);
  } else if (length < 0 || offset + length > bufferLength) {
    err = new ERR_OUT_OF_RANGE('length',
                               `>= 0 && <= ${bufferLength - offset}`, length);
  }

  if (err !== undefined) {
    Error.captureStackTrace(err, validateOffsetLengthRead);
    throw err;
  }
}

function validateOffsetLengthWrite(offset, length, byteLength) {
  let err;

  if (offset > byteLength) {
    err = new ERR_OUT_OF_RANGE('offset', `<= ${byteLength}`, offset);
  } else {
    const max = byteLength > kMaxLength ? kMaxLength : byteLength;
    if (length > max - offset) {
      err = new ERR_OUT_OF_RANGE('length', `<= ${max - offset}`, length);
    }
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

module.exports = {
  assertEncoding,
  copyObject,
  Dirent,
  getDirents,
  getOptions,
  nullCheck,
  preprocessSymlinkDestination,
  realpathCacheKey: Symbol('realpathCacheKey'),
  getStatsFromBinding,
  stringToFlags,
  stringToSymlinkType,
  Stats,
  toUnixTimestamp,
  validateBuffer,
  validateOffsetLengthRead,
  validateOffsetLengthWrite,
  validatePath
};
