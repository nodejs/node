'use strict';

process.emitWarning(
  'The promisified fs module is an experimental API.',
  'ExperimentalWarning'
);

const {
  F_OK,
  O_SYMLINK,
  O_WRONLY
} = process.binding('constants').fs;

const errors = require('internal/errors');

const {
  access: _access,
  chmod: _chmod,
  chown: _chown,
  close: _close,
  copyFile: _copyFile,
  fchmod: _fchmod,
  fchown: _fchown,
  fdatasync: _fdatasync,
  fstat: _fstat,
  fsync: _fsync,
  ftruncate: _ftruncate,
  futimes: _futimes,
  link: _link,
  lstat: _lstat,
  mkdir: _mkdir,
  mkdtemp: _mkdtemp,
  open: _open,
  read: _read,
  readdir: _readdir,
  readlink: _readlink,
  realpath: _realpath,
  rename: _rename,
  rmdir: _rmdir,
  stat: _stat,
  symlink: _symlink,
  unlink: _unlink,
  utimes: _utimes,
  writeBuffer: _writeBuffer,
  writeString: _writeString,
  FSReqWrap
} = process.binding('fs');

const {
  assertEncoding,
  getOptions,
  modeNum,
  nullCheck,
  preprocessSymlinkDestination,
  statsFromValues,
  stringToFlags,
  stringToSymlinkType,
  toUnixTimestamp
} = require('internal/fs');

const {
  createPromise,
  isUint8Array,
  promiseResolve,
  promiseReject
} = process.binding('util');

const {
  _makeLong
} = require('path');

const {
  getPathFromURL
} = require('internal/url');

const {
  Buffer,
  kMaxLength
} = require('buffer');

const kReadFileBufferLength = 8 * 1024;

function handleError(val) {
  if (val instanceof Error)
    throw val;
}

function isFd(path) {
  return (path >>> 0) === path;
}

function access(path, mode = F_OK) {
  const promise = createPromise();
  try {
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
    req.oncomplete = promise;
    _access(_makeLong(path), mode, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function chmod(path, mode) {
  const promise = createPromise();
  try {
    mode = modeNum(mode);
    handleError(path = getPathFromURL(path));
    nullCheck(path);

    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }
    if (!Number.isInteger(mode) || mode < 0) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'mode',
                                 'unsigned integer');
    }

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _chmod(_makeLong(path), mode, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function chown(path, uid, gid) {
  const promise = createPromise();
  try {
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

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _chown(_makeLong(path), uid, gid, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function close(fd) {
  const promise = createPromise();
  try {
    if (!Number.isInteger(fd) || fd < 0) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                                 'unsigned integer');
    }
    const req = new FSReqWrap();
    req.oncomplete = promise;
    _close(fd, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function copyFile(src, dest, flags = 0) {
  const promise = createPromise();
  try {
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

    src = _makeLong(src);
    dest = _makeLong(dest);
    flags = flags | 0;
    const req = new FSReqWrap();
    req.oncomplete = promise;
    _copyFile(src, dest, flags, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function fchmod(fd, mode) {
  const promise = createPromise();
  try {
    mode = modeNum(mode);
    if (!Number.isInteger(fd) || fd < 0) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                                 'unsigned integer');
    }
    const req = new FSReqWrap();
    req.oncomplete = promise;
    _fchmod(fd, mode, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function fchown(fd, uid, gid) {
  const promise = createPromise();
  try {
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
    req.oncomplete = promise;
    _fchown(fd, uid, gid, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function fdatasync(fd) {
  const promise = createPromise();
  try {
    if (!Number.isInteger(fd) || fd < 0) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                                 'unsigned integer');
    }

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _fdatasync(fd, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function fsync(fd) {
  const promise = createPromise();
  try {
    if (!Number.isInteger(fd) || fd < 0) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                                 'unsigned integer');
    }
    const req = new FSReqWrap();
    req.oncomplete = promise;
    _fsync(fd, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function ftruncate(fd, len) {
  const promise = createPromise();
  try {
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

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _ftruncate(fd, len, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function futimes(fd, atime, mtime) {
  const promise = createPromise();
  try {
    if (!Number.isInteger(fd) || fd < 0) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                                 'unsigned integer');
    }

    atime = toUnixTimestamp(atime);
    mtime = toUnixTimestamp(mtime);
    const req = new FSReqWrap();
    req.oncomplete = promise;
    _futimes(fd, atime, mtime, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function open(path, flags, mode) {
  const promise = createPromise();
  try {
    mode = modeNum(mode, 0o666);
    flags = stringToFlags(flags);

    handleError(path = getPathFromURL(path));
    nullCheck(path);

    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }

    const req = new FSReqWrap();
    req.oncomplete = promise;

    _open(_makeLong(path), flags, mode, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function link(existingPath, newPath) {
  const promise = createPromise();
  try {
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
    req.oncomplete = promise;

    _link(_makeLong(existingPath),
          _makeLong(newPath),
          req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function mkdir(path, mode) {
  const promise = createPromise();
  try {
    mode = modeNum(mode, 0o777);
    handleError(path = getPathFromURL(path));
    nullCheck(path);

    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _mkdir(_makeLong(path), mode, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function mkdtemp(prefix, options) {
  const promise = createPromise();
  try {
    options = getOptions(options, {});
    if (!prefix || typeof prefix !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'prefix',
                                 'string');
    }
    nullCheck(prefix);

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _mkdtemp(`${prefix}XXXXXX`, options.encoding, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function readdir(path, options) {
  const promise = createPromise();
  try {
    options = getOptions(options, {});
    handleError(path = getPathFromURL(path));
    nullCheck(path);

    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _readdir(_makeLong(path), options.encoding, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function readlink(path, options) {
  const promise = createPromise();
  try {
    options = getOptions(options, {});
    handleError(path = getPathFromURL(path));
    nullCheck(path);

    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _readlink(_makeLong(path), options.encoding, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function rename(oldPath, newPath) {
  const promise = createPromise();
  try {
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
    req.oncomplete = promise;

    _rename(_makeLong(oldPath),
            _makeLong(newPath),
            req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function rmdir(path) {
  const promise = createPromise();
  try {
    handleError(path = getPathFromURL(path));
    nullCheck(path);

    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _rmdir(_makeLong(path), req);
  } catch (err) {
    promiseReject(err);
  }
  return promise;
}

function symlink(target, path, type = 'file') {
  const promise = createPromise();
  try {
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
    req.oncomplete = promise;

    _symlink(preprocessSymlinkDestination(target, type, path),
             _makeLong(path),
             _type,
             req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function unlink(path) {
  const promise = createPromise();
  try {
    handleError(path = getPathFromURL(path));
    nullCheck(path);

    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }

    const req = new FSReqWrap();
    req.oncomplete = promise;
    _unlink(_makeLong(path), req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function utimes(path, atime, mtime) {
  const promise = createPromise();
  try {
    handleError(path = getPathFromURL(path));
    nullCheck(path);
    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }
    const req = new FSReqWrap();
    req.oncomplete = promise;
    _utimes(_makeLong(path),
            toUnixTimestamp(atime),
            toUnixTimestamp(mtime),
            req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function writeBuffer(fd, buffer, offset, length, position, req) {
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

  _writeBuffer(fd, buffer, offset, length, position, req);
}

function writeString(fd, str, position, encoding, req) {
  assertEncoding(encoding);
  if (typeof position !== 'number')
    position = null;
  _writeString(fd, `${str}`, position, encoding || 'utf8', req);
}

function write(fd, buffer, offsetOrPosition, lengthOrEncoding, position) {
  const promise = createPromise();
  try {
    if (!Number.isInteger(fd) || fd < 0) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                                 'unsigned integer');
    }
    const req = new FSReqWrap();
    req.oncomplete = promise;
    req.buffer = buffer; // Retain a reference to buffer

    if (isUint8Array(buffer)) {
      writeBuffer(fd, buffer, offsetOrPosition,
                  lengthOrEncoding, position, req);
    } else {
      writeString(fd, buffer, offsetOrPosition, lengthOrEncoding, req);
    }
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

async function writeAll(fd, isUserFd, buffer, offset, length, position) {
  try {
    const written = await write(fd, buffer, offset, length, position);
    if (written === length) {
      if (!isUserFd)
        await close(fd);
    } else {
      offset += written;
      length -= written;
      if (position !== null)
        position += written;
      await writeAll(fd, isUserFd, buffer, offset, length, position);
    }
    return written;
  } catch (err) {
    if (!isUserFd)
      await close(fd);
    throw err;
  }
}

async function writeFd(fd, data, isUserFd, options, flag) {
  const buffer = isUint8Array(data) ?
    data : Buffer.from(`${data}`, options.encoding || 'utf8');
  const position = /a/.test(flag) ? null : 0;
  return await writeAll(fd, isUserFd, buffer, 0, buffer.length, position);
}

async function writeFile(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'w' });
  const flag = options.flag || 'w';

  if (isFd(path)) {
    return await writeFd(path, data, true, options, flag);
  } else {
    const fd = await open(path, flag, options, options.mode);
    return await writeFd(fd, data, false, options, flag);
  }
}

async function appendFile(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'a' });
  options = Object.assign({}, options);
  if (!options.flag || isFd(path))
    options.flag = 'a';
  return await writeFile(path, data, options);
}

async function truncate(path, len = 0) {
  if (typeof path === 'number')
    return await ftruncate(path, len);

  const fd = await open(path, 'r+');
  try {
    await ftruncate(fd, len);
  } finally {
    await close(fd);
  }
}

function read(fd, buffer, offset, length, position) {
  const promise = createPromise();
  try {
    if (!Number.isInteger(fd) || fd < 0) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                                 'unsigned integer');
    }
    if (!isUint8Array(buffer)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'buffer',
                                 ['Buffer', 'Uint8Array']);
    }
    if (length === 0) {
      promiseResolve(promise, { length: 0, buffer });
      return promise;
    }

    function wrapper(err, len) {
      if (err) {
        promiseReject(promise, err);
        return;
      }
      promiseResolve(promise, { length: len, buffer });
    }

    if (typeof offset !== 'number')
      offset = 0;
    if (typeof length !== 'number')
      length = buffer.length;
    if (typeof position !== 'number')
      position = 0;

    if (offset > buffer.length)
      throw new errors.RangeError('ERR_OUTOFBOUNDS', 'offset');
    if (length > buffer.length)
      throw new errors.RangeError('ERR_OUTOFBOUNDS', 'length');

    const req = new FSReqWrap();
    req.oncomplete = wrapper;

    _read(fd, buffer, offset, length, position, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function resolveWithStats(promise) {
  return (err) => {
    if (err) {
      promiseReject(promise, err);
      return;
    }
    promiseResolve(promise, statsFromValues());
  };
}

function fstat(fd) {
  const promise = createPromise();
  try {
    if (!Number.isInteger(fd) || fd < 0) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fd',
                                 'unsigned integer');
    }
    const req = new FSReqWrap();
    req.oncomplete = resolveWithStats(promise);
    _fstat(fd, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function lstat(path) {
  const promise = createPromise();
  try {
    handleError(path = getPathFromURL(path));
    nullCheck(path);
    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }
    const req = new FSReqWrap();
    req.oncomplete = resolveWithStats(promise);
    _lstat(_makeLong(path), req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

function stat(path) {
  const promise = createPromise();
  try {
    handleError(path = getPathFromURL(path));
    nullCheck(path);
    if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                                 ['string', 'Buffer', 'URL']);
    }
    const req = new FSReqWrap();
    req.oncomplete = resolveWithStats(promise);
    _stat(_makeLong(path), req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

async function lchmod(path, mode) {
  const fd = await open(path, O_WRONLY | O_SYMLINK);
  try {
    await fchmod(fd, mode);
  } finally {
    await close(fd);
  }
}

async function lchown(path, uid, gid) {
  const fd = await open(path, O_WRONLY | O_SYMLINK);
  try {
    await fchown(fd, uid, gid);
  } finally {
    await close(fd);
  }
}

// This uses the fast libuv implementation, not the legacy one that
// is in fs that we had to revert back to.
function realpath(path, options) {
  options = getOptions(options, { encoding: 'utf8' });

  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  const promise = createPromise();
  const req = new FSReqWrap();
  req.oncomplete = promise;
  try {
    _realpath(_makeLong(path), options.encoding, req);
  } catch (err) {
    promiseReject(promise, err);
  }
  return promise;
}

async function readFile(path, options) {
  options = getOptions(options, { flag: 'r' });

  handleError(path = getPathFromURL(path));
  nullCheck(path);
  if (typeof path !== 'string' && !Buffer.isBuffer(path)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'path',
                               ['string', 'Buffer', 'URL']);
  }

  const fd = isFd(path) ?
    path :
    await open(_makeLong(path),
               stringToFlags(options.flag || 'r'),
               0o666);

  try {
    const stat = await fstat(fd);

    const size = stat.isFile() ? stat.size : 0;

    if (size > kMaxLength) {
      throw new RangeError('File size is greater than possible Buffer: ' +
                          `0x${kMaxLength.toString(16)} bytes`);
    }

    // Ok, so if size it known, allocate a buffer that big and read once.
    // Otherwise, iterate until there is no more data to read, concat
    // the buffers, and done!
    let buffer;
    if (size > 0) {
      buffer = Buffer.allocUnsafeSlow(size);
      await read(fd, buffer, 0, size);
    } else {
      // The size is not known, just read until there are no more bytes
      const buffers = [];
      let bytesRead = 0;
      do {
        const buf = Buffer.allocUnsafeSlow(kReadFileBufferLength);
        bytesRead =
          (await read(fd, buf, 0, kReadFileBufferLength, null)).length;
        buffers.push(buf.slice(0, bytesRead));
      } while (bytesRead > 0);
      buffer = Buffer.concat(buffers);
    }

    if (options.encoding)
      return buffer.toString(options.encoding);

    return buffer;
  } finally {
    await close(fd);
  }
}

module.exports = {
  access,
  appendFile,
  chmod,
  chown,
  close,
  copyFile,
  fchmod,
  fchown,
  fdatasync,
  fstat,
  fsync,
  ftruncate,
  futimes,
  open,
  link,
  lstat,
  mkdir,
  mkdtemp,
  read,
  readdir,
  readFile,
  readlink,
  realpath,
  rename,
  rmdir,
  stat,
  symlink,
  truncate,
  unlink,
  utimes,
  write,
  writeFile
};

if (O_SYMLINK !== undefined) {
  module.exports.lchmod = lchmod;
  module.exports.lchown = lchown;
}
