'use strict';

const {
  F_OK,
  O_SYMLINK,
  O_WRONLY,
  S_IFMT,
  S_IFREG
} = process.binding('constants').fs;
const binding = process.binding('fs');
const { Buffer, kMaxLength } = require('buffer');
const {
  ERR_FS_FILE_TOO_LARGE,
  ERR_INVALID_ARG_TYPE,
  ERR_METHOD_NOT_IMPLEMENTED
} = require('internal/errors').codes;
const { getPathFromURL } = require('internal/url');
const { isUint8Array } = require('internal/util/types');
const {
  copyObject,
  getOptions,
  getStatsFromBinding,
  nullCheck,
  preprocessSymlinkDestination,
  stringToFlags,
  stringToSymlinkType,
  toUnixTimestamp,
  validateBuffer,
  validateOffsetLengthRead,
  validateOffsetLengthWrite,
  validatePath
} = require('internal/fs/utils');
const {
  validateMode,
  validateInteger,
  validateUint32
} = require('internal/validators');
const pathModule = require('path');

const kHandle = Symbol('handle');
const { kUsePromises } = binding;

class FileHandle {
  constructor(filehandle) {
    this[kHandle] = filehandle;
  }

  getAsyncId() {
    return this[kHandle].getAsyncId();
  }

  get fd() {
    return this[kHandle].fd;
  }

  appendFile(data, options) {
    return appendFile(this, data, options);
  }

  chmod(mode) {
    return fchmod(this, mode);
  }

  chown(uid, gid) {
    return fchown(this, uid, gid);
  }

  datasync() {
    return fdatasync(this);
  }

  sync() {
    return fsync(this);
  }

  read(buffer, offset, length, position) {
    return read(this, buffer, offset, length, position);
  }

  readFile(options) {
    return readFile(this, options);
  }

  stat(options) {
    return fstat(this, options);
  }

  truncate(len = 0) {
    return ftruncate(this, len);
  }

  utimes(atime, mtime) {
    return futimes(this, atime, mtime);
  }

  write(buffer, offset, length, position) {
    return write(this, buffer, offset, length, position);
  }

  writeFile(data, options) {
    return writeFile(this, data, options);
  }

  close() {
    return this[kHandle].close();
  }
}

function validateFileHandle(handle) {
  if (!(handle instanceof FileHandle))
    throw new ERR_INVALID_ARG_TYPE('filehandle', 'FileHandle', handle);
}

async function writeFileHandle(filehandle, data, options) {
  let buffer = isUint8Array(data) ?
    data : Buffer.from('' + data, options.encoding || 'utf8');
  let remaining = buffer.length;
  if (remaining === 0) return;
  do {
    const { bytesWritten } =
      await write(filehandle, buffer, 0,
                  Math.min(16384, buffer.length));
    remaining -= bytesWritten;
    buffer = buffer.slice(bytesWritten);
  } while (remaining > 0);
}

async function readFileHandle(filehandle, options) {
  const statFields = await binding.fstat(filehandle.fd, false, kUsePromises);

  let size;
  if ((statFields[1/* mode */] & S_IFMT) === S_IFREG) {
    size = statFields[8/* size */];
  } else {
    size = 0;
  }

  if (size === 0)
    return options.encoding ? '' : Buffer.alloc(0);

  if (size > kMaxLength)
    throw new ERR_FS_FILE_TOO_LARGE(size);

  const chunks = [];
  const chunkSize = Math.min(size, 16384);
  let totalRead = 0;
  let endOfFile = false;
  do {
    const buf = Buffer.alloc(chunkSize);
    const { bytesRead, buffer } =
      await read(filehandle, buf, 0, chunkSize, totalRead);
    totalRead += bytesRead;
    endOfFile = bytesRead !== chunkSize;
    if (bytesRead > 0)
      chunks.push(buffer.slice(0, bytesRead));
  } while (!endOfFile);

  const result = Buffer.concat(chunks);
  if (options.encoding) {
    return result.toString(options.encoding);
  } else {
    return result;
  }
}

// All of the functions are defined as async in order to ensure that errors
// thrown cause promise rejections rather than being thrown synchronously.
async function access(path, mode = F_OK) {
  path = getPathFromURL(path);
  validatePath(path);

  mode = mode | 0;
  return binding.access(pathModule.toNamespacedPath(path), mode,
                        kUsePromises);
}

async function copyFile(src, dest, flags) {
  src = getPathFromURL(src);
  dest = getPathFromURL(dest);
  validatePath(src, 'src');
  validatePath(dest, 'dest');
  flags = flags | 0;
  return binding.copyFile(pathModule.toNamespacedPath(src),
                          pathModule.toNamespacedPath(dest),
                          flags, kUsePromises);
}

// Note that unlike fs.open() which uses numeric file descriptors,
// fsPromises.open() uses the fs.FileHandle class.
async function open(path, flags, mode) {
  path = getPathFromURL(path);
  validatePath(path);
  mode = validateMode(mode, 'mode', 0o666);
  return new FileHandle(
    await binding.openFileHandle(pathModule.toNamespacedPath(path),
                                 stringToFlags(flags),
                                 mode, kUsePromises));
}

async function read(handle, buffer, offset, length, position) {
  validateFileHandle(handle);
  validateBuffer(buffer);

  offset |= 0;
  length |= 0;

  if (length === 0)
    return { bytesRead: length, buffer };

  validateOffsetLengthRead(offset, length, buffer.length);

  if (!Number.isSafeInteger(position))
    position = -1;

  const bytesRead = (await binding.read(handle.fd, buffer, offset, length,
                                        position, kUsePromises)) || 0;

  return { bytesRead, buffer };
}

async function write(handle, buffer, offset, length, position) {
  validateFileHandle(handle);

  if (buffer.length === 0)
    return { bytesWritten: 0, buffer };

  if (isUint8Array(buffer)) {
    if (typeof offset !== 'number')
      offset = 0;
    if (typeof length !== 'number')
      length = buffer.length - offset;
    if (typeof position !== 'number')
      position = null;
    validateOffsetLengthWrite(offset, length, buffer.byteLength);
    const bytesWritten =
      (await binding.writeBuffer(handle.fd, buffer, offset,
                                 length, position, kUsePromises)) || 0;
    return { bytesWritten, buffer };
  }

  if (typeof buffer !== 'string')
    buffer += '';
  const bytesWritten = (await binding.writeString(handle.fd, buffer, offset,
                                                  length, kUsePromises)) || 0;
  return { bytesWritten, buffer };
}

async function rename(oldPath, newPath) {
  oldPath = getPathFromURL(oldPath);
  newPath = getPathFromURL(newPath);
  validatePath(oldPath, 'oldPath');
  validatePath(newPath, 'newPath');
  return binding.rename(pathModule.toNamespacedPath(oldPath),
                        pathModule.toNamespacedPath(newPath),
                        kUsePromises);
}

async function truncate(path, len = 0) {
  return ftruncate(await open(path, 'r+'), len);
}

async function ftruncate(handle, len = 0) {
  validateFileHandle(handle);
  validateInteger(len, 'len');
  len = Math.max(0, len);
  return binding.ftruncate(handle.fd, len, kUsePromises);
}

async function rmdir(path) {
  path = getPathFromURL(path);
  validatePath(path);
  return binding.rmdir(pathModule.toNamespacedPath(path), kUsePromises);
}

async function fdatasync(handle) {
  validateFileHandle(handle);
  return binding.fdatasync(handle.fd, kUsePromises);
}

async function fsync(handle) {
  validateFileHandle(handle);
  return binding.fsync(handle.fd, kUsePromises);
}

async function mkdir(path, mode) {
  path = getPathFromURL(path);
  validatePath(path);
  mode = validateMode(mode, 'mode', 0o777);
  return binding.mkdir(pathModule.toNamespacedPath(path), mode, kUsePromises);
}

async function readdir(path, options) {
  options = getOptions(options, {});
  path = getPathFromURL(path);
  validatePath(path);
  return binding.readdir(pathModule.toNamespacedPath(path),
                         options.encoding, kUsePromises);
}

async function readlink(path, options) {
  options = getOptions(options, {});
  path = getPathFromURL(path);
  validatePath(path, 'oldPath');
  return binding.readlink(pathModule.toNamespacedPath(path),
                          options.encoding, kUsePromises);
}

async function symlink(target, path, type_) {
  const type = (typeof type_ === 'string' ? type_ : null);
  target = getPathFromURL(target);
  path = getPathFromURL(path);
  validatePath(target, 'target');
  validatePath(path);
  return binding.symlink(preprocessSymlinkDestination(target, type, path),
                         pathModule.toNamespacedPath(path),
                         stringToSymlinkType(type),
                         kUsePromises);
}

async function fstat(handle, options = { bigint: false }) {
  validateFileHandle(handle);
  const result = await binding.fstat(handle.fd, options.bigint, kUsePromises);
  return getStatsFromBinding(result);
}

async function lstat(path, options = { bigint: false }) {
  path = getPathFromURL(path);
  validatePath(path);
  const result = await binding.lstat(pathModule.toNamespacedPath(path),
                                     options.bigint, kUsePromises);
  return getStatsFromBinding(result);
}

async function stat(path, options = { bigint: false }) {
  path = getPathFromURL(path);
  validatePath(path);
  const result = await binding.stat(pathModule.toNamespacedPath(path),
                                    options.bigint, kUsePromises);
  return getStatsFromBinding(result);
}

async function link(existingPath, newPath) {
  existingPath = getPathFromURL(existingPath);
  newPath = getPathFromURL(newPath);
  validatePath(existingPath, 'existingPath');
  validatePath(newPath, 'newPath');
  return binding.link(pathModule.toNamespacedPath(existingPath),
                      pathModule.toNamespacedPath(newPath),
                      kUsePromises);
}

async function unlink(path) {
  path = getPathFromURL(path);
  validatePath(path);
  return binding.unlink(pathModule.toNamespacedPath(path), kUsePromises);
}

async function fchmod(handle, mode) {
  validateFileHandle(handle);
  mode = validateMode(mode, 'mode');
  return binding.fchmod(handle.fd, mode, kUsePromises);
}

async function chmod(path, mode) {
  path = getPathFromURL(path);
  validatePath(path);
  mode = validateMode(mode, 'mode');
  return binding.chmod(pathModule.toNamespacedPath(path), mode, kUsePromises);
}

async function lchmod(path, mode) {
  if (O_SYMLINK === undefined)
    throw new ERR_METHOD_NOT_IMPLEMENTED();

  const fd = await open(path, O_WRONLY | O_SYMLINK);
  return fchmod(fd, mode).finally(fd.close.bind(fd));
}

async function lchown(path, uid, gid) {
  if (O_SYMLINK === undefined)
    throw new ERR_METHOD_NOT_IMPLEMENTED();

  const fd = await open(path, O_WRONLY | O_SYMLINK);
  return fchown(fd, uid, gid).finally(fd.close.bind(fd));
}

async function fchown(handle, uid, gid) {
  validateFileHandle(handle);
  validateUint32(uid, 'uid');
  validateUint32(gid, 'gid');
  return binding.fchown(handle.fd, uid, gid, kUsePromises);
}

async function chown(path, uid, gid) {
  path = getPathFromURL(path);
  validatePath(path);
  validateUint32(uid, 'uid');
  validateUint32(gid, 'gid');
  return binding.chown(pathModule.toNamespacedPath(path),
                       uid, gid, kUsePromises);
}

async function utimes(path, atime, mtime) {
  path = getPathFromURL(path);
  validatePath(path);
  return binding.utimes(pathModule.toNamespacedPath(path),
                        toUnixTimestamp(atime),
                        toUnixTimestamp(mtime),
                        kUsePromises);
}

async function futimes(handle, atime, mtime) {
  validateFileHandle(handle);
  atime = toUnixTimestamp(atime, 'atime');
  mtime = toUnixTimestamp(mtime, 'mtime');
  return binding.futimes(handle.fd, atime, mtime, kUsePromises);
}

async function realpath(path, options) {
  options = getOptions(options, {});
  path = getPathFromURL(path);
  validatePath(path);
  return binding.realpath(path, options.encoding, kUsePromises);
}

async function mkdtemp(prefix, options) {
  options = getOptions(options, {});
  if (!prefix || typeof prefix !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('prefix', 'string', prefix);
  }
  nullCheck(prefix);
  return binding.mkdtemp(`${prefix}XXXXXX`, options.encoding, kUsePromises);
}

async function writeFile(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'w' });
  const flag = options.flag || 'w';

  if (path instanceof FileHandle)
    return writeFileHandle(path, data, options);

  const fd = await open(path, flag, options.mode);
  return writeFileHandle(fd, data, options).finally(fd.close.bind(fd));
}

async function appendFile(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'a' });
  options = copyObject(options);
  options.flag = options.flag || 'a';
  return writeFile(path, data, options);
}

async function readFile(path, options) {
  options = getOptions(options, { flag: 'r' });
  const flag = options.flag || 'r';

  if (path instanceof FileHandle)
    return readFileHandle(path, options);

  const fd = await open(path, flag, 0o666);
  return readFileHandle(fd, options).finally(fd.close.bind(fd));
}

module.exports = {
  access,
  copyFile,
  open,
  rename,
  truncate,
  rmdir,
  mkdir,
  readdir,
  readlink,
  symlink,
  lstat,
  stat,
  link,
  unlink,
  chmod,
  lchmod,
  lchown,
  chown,
  utimes,
  realpath,
  mkdtemp,
  writeFile,
  appendFile,
  readFile
};
