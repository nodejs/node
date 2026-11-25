'use strict';

const {
  ArrayPrototypePush,
  BigIntPrototypeToString,
  ObjectDefineProperty,
  SafeMap,
  SafeSet,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  SymbolDispose,
  uncurryThis,
} = primordials;

const { fs: constants } = internalBinding('constants');
const {
  S_IFIFO,
  S_IFLNK,
  S_IFREG,
  S_IFSOCK,
  O_WRONLY,
  O_SYMLINK,
} = constants;

const pathModule = require('path');
const { isArrayBufferView } = require('internal/util/types');
const binding = internalBinding('fs');
const { Buffer } = require('buffer');
const { isBuffer: BufferIsBuffer } = Buffer;
const BufferToString = uncurryThis(Buffer.prototype.toString);
const {
  codes: {
    ERR_ACCESS_DENIED,
    ERR_FS_FILE_TOO_LARGE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const { toPathIfFileURL } = require('internal/url');
const {
  kEmptyObject,
  isWindows,
} = require('internal/util');
const {
  constants: {
    kIoMaxLength,
    kMaxUserId,
  },
  collectRecursiveReaddirResult,
  copyObject,
  encodeRealpathResult,
  getRealpathRootLstatPath,
  getDirents,
  getRecursiveDirents,
  getOptions,
  handleErrorFromBinding,
  isFileType,
  joinRealpathLink,
  joinRealpathRemainder,
  lazyGlob,
  preprocessSymlinkDestination,
  getReadFileBuffer,
  getReadFileBufferByteLengthName,
  getStatFsFromBinding,
  getStatsFromBinding,
  realpathCacheKey,
  stringToFlags,
  stringToSymlinkType,
  toUnixTimestamp,
  validateBufferArray,
  validateCpOptions,
  validateOffsetLengthRead,
  validateOffsetLengthWrite,
  validatePath,
  validatePosition,
  validateReadFileBufferOptions,
  validateRmOptionsSync,
  validateRmdirOptions,
  validateStringAfterArrayBufferView,
  getValidatedPath,
  nextPart,
  splitRoot,
  toRealpathInput,
  vfsState,
  warnOnNonPortableTemplate,
} = require('internal/fs/utils');
const {
  CHAR_FORWARD_SLASH,
  CHAR_BACKWARD_SLASH,
} = require('internal/constants');
const {
  isInt32,
  parseFileMode,
  validateBoolean,
  validateBuffer,
  validateEncoding,
  validateInteger,
  validateObject,
  validateOneOf,
  validateString,
  kValidateObjectAllowNullable,
} = require('internal/validators');
const permission = require('internal/process/permission');

const isFd = isInt32;

let cpSyncFn;
function lazyLoadCp() {
  return cpSyncFn ??= require('internal/fs/cp/cp-sync').cpSyncFn;
}

// TODO(LiviaMedeiros): remove this once monkey-patching is disallowed
// Helper object to be replaced with userland-facing `fs` to allow
// monkey-patching it.
let fs;
function setFS(fsModule) {
  fs = fsModule;
}

/**
 * Synchronously tests a user's permissions for the file or
 * directory specified by `path`.
 * @param {string | Buffer | URL} path
 * @param {number} [mode]
 * @returns {void}
 */
function accessSync(path, mode) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.accessSync(path, mode);
    if (result !== undefined) return;
  }
  binding.access(getValidatedPath(path), mode);
}

/**
 * Synchronously tests whether or not the given path exists.
 * @param {string | Buffer | URL} path
 * @returns {boolean}
 * @throws {ERR_INVALID_ARG_TYPE} If `path` is not a string, Buffer, or URL.
 */
function existsSync(path) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.existsSync(path);
    if (result !== undefined) return result;
  }
  try {
    path = getValidatedPath(path);
  } catch (err) {
    if (err?.code === 'ERR_INVALID_ARG_TYPE') throw err;
    return false;
  }

  return binding.existsSync(path);
}

function tryStatSync(fd, isUserFd) {
  const stats = binding.fstat(fd, false, undefined, true /* shouldNotThrow */);
  if (stats === undefined && !isUserFd) {
    fs.closeSync(fd);
  }
  return stats;
}

function tryCreateBuffer(size, fd, isUserFd) {
  let threw = true;
  let buffer;
  try {
    if (size > kIoMaxLength) {
      throw new ERR_FS_FILE_TOO_LARGE(size);
    }
    buffer = Buffer.allocUnsafe(size);
    threw = false;
  } finally {
    if (threw && !isUserFd) fs.closeSync(fd);
  }
  return buffer;
}

function tryGetReadFileBuffer(options, size, fd, isUserFd) {
  let threw = true;
  let buffer;
  try {
    buffer = getReadFileBuffer(options, size);
    threw = false;
  } finally {
    if (threw && !isUserFd) fs.closeSync(fd);
  }
  return buffer;
}

function tryReadSync(fd, isUserFd, buffer, pos, len) {
  let threw = true;
  let bytesRead;
  try {
    bytesRead = fs.readSync(fd, buffer, pos, len);
    threw = false;
  } finally {
    if (threw && !isUserFd) fs.closeSync(fd);
  }
  return bytesRead;
}

function tryReadSyncWithUserBuffer(fd, isUserFd, buffer, byteLengthName) {
  let pos = 0;
  let bytesRead = 0;

  while (pos < buffer.byteLength) {
    bytesRead = tryReadSync(fd, isUserFd, buffer, pos, buffer.byteLength - pos);
    pos += bytesRead;

    if (bytesRead === 0) {
      return pos;
    }
  }

  const extraBuffer = tryCreateBuffer(1, fd, isUserFd);
  bytesRead = tryReadSync(fd, isUserFd, extraBuffer, 0, 1);

  if (bytesRead !== 0) {
    if (!isUserFd) {
      fs.closeSync(fd);
    }
    throw new ERR_INVALID_ARG_VALUE(
      byteLengthName,
      buffer.byteLength,
      'is too small to contain the entire file',
    );
  }

  return pos;
}

/**
 * Synchronously reads the entire contents of a file.
 * @param {string | Buffer | URL | number} path
 * @param {{
 *   encoding?: string | null;
 *   flag?: string;
 *   }} [options]
 * @returns {string | Buffer}
 */
function readFileSync(path, options) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.readFileSync(path, options);
    if (result !== undefined) return result;
  }
  options = getOptions(options, { flag: 'r' });
  validateReadFileBufferOptions(options);
  const hasUserBuffer = options.buffer !== undefined;

  if ((options.encoding === 'utf8' || options.encoding === 'utf-8') &&
      !hasUserBuffer) {
    if (isInt32(path)) {
      // V8 does not report -0 as an int32, so it would reach the binding as a
      // path instead of a file descriptor.
      path |= 0;
    } else {
      path = getValidatedPath(path);
    }
    return binding.readFileUtf8(path, stringToFlags(options.flag));
  }

  const isUserFd = isFd(path); // File descriptor ownership
  const fd = isUserFd ? path : fs.openSync(path, options.flag, 0o666);

  const stats = tryStatSync(fd, isUserFd);
  const size = isFileType(stats, S_IFREG) ? stats[8] : 0;
  let pos = 0;
  let buffer; // Single buffer with file data
  let buffers; // List for when size is unknown

  if (hasUserBuffer) {
    buffer = tryGetReadFileBuffer(options, size, fd, isUserFd);
  } else if (size === 0) {
    buffers = [];
  } else {
    buffer = tryCreateBuffer(size, fd, isUserFd);
  }

  let bytesRead;

  if (hasUserBuffer) {
    if (size !== 0) {
      do {
        bytesRead = tryReadSync(fd, isUserFd, buffer, pos, size - pos);
        pos += bytesRead;
      } while (bytesRead !== 0 && pos < size);
    } else {
      pos = tryReadSyncWithUserBuffer(
        fd,
        isUserFd,
        buffer,
        getReadFileBufferByteLengthName(options),
      );
    }
  } else if (size !== 0) {
    do {
      bytesRead = tryReadSync(fd, isUserFd, buffer, pos, size - pos);
      pos += bytesRead;
    } while (bytesRead !== 0 && pos < size);
  } else {
    do {
      // The kernel lies about many files.
      // Go ahead and try to read some bytes.
      buffer = Buffer.allocUnsafe(8192);
      bytesRead = tryReadSync(fd, isUserFd, buffer, 0, 8192);
      if (bytesRead !== 0) {
        ArrayPrototypePush(buffers, buffer.slice(0, bytesRead));
      }
      pos += bytesRead;
    } while (bytesRead !== 0);
  }

  if (!isUserFd)
    fs.closeSync(fd);

  if (hasUserBuffer) {
    buffer = buffer.subarray(0, pos);
  } else if (size === 0) {
    // Data was collected into the buffers list.
    buffer = Buffer.concat(buffers, pos);
  } else if (pos < size) {
    buffer = buffer.slice(0, pos);
  }

  if (options.encoding) buffer = buffer.toString(options.encoding);
  return buffer;
}

/**
 * Synchronously closes the file descriptor.
 * @param {number} fd
 * @returns {void}
 */
function closeSync(fd) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.closeSync(fd);
    if (result !== undefined) return;
  }
  binding.close(fd);
}

/**
 * Synchronously opens a file.
 * @param {string | Buffer | URL} path
 * @param {string | number} [flags]
 * @param {string | number} [mode]
 * @returns {number}
 */
function openSync(path, flags, mode) {
  flags = stringToFlags(flags);
  mode = parseFileMode(mode, 'mode', 0o666);
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.openSync(path, flags, mode);
    if (result !== undefined) return result;
  }
  return binding.open(
    getValidatedPath(path),
    flags,
    mode,
  );
}

/**
 * @param {string | Buffer | URL} path
 * @param {{
 *   type?: string;
 *   }} [options]
 * @returns {Blob}
 */
function openAsBlobSync(path, options = kEmptyObject) {
  validateObject(options, 'options');
  const { type = '' } = options;
  validateString(type, 'options.type');
  path = getValidatedPath(path);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.openAsBlobSync(path, options);
    if (result !== undefined) return result;
  }

  const { createBlobFromFilePath } = require('internal/blob');
  return createBlobFromFilePath(path, { type });
}

/**
 * Synchronously reads the file from the
 * specified `fd` (file descriptor).
 * @param {number} fd
 * @param {Buffer | TypedArray | DataView} buffer
 * @param {number | {
 *   offset?: number;
 *   length?: number;
 *   position?: number | bigint | null;
 *   }} [offsetOrOptions]
 * @param {number} [length]
 * @param {number} [position]
 * @returns {number}
 */
function readSync(fd, buffer, offsetOrOptions, length, position) {
  validateBuffer(buffer);

  let offset = offsetOrOptions;
  if (arguments.length <= 3 || typeof offsetOrOptions === 'object') {
    if (offsetOrOptions !== undefined) {
      validateObject(offsetOrOptions, 'options', kValidateObjectAllowNullable);
    }

    ({
      offset = 0,
      length = buffer.byteLength - offset,
      position = null,
    } = offsetOrOptions ?? kEmptyObject);
  }

  if (offset === undefined) {
    offset = 0;
  } else {
    validateInteger(offset, 'offset', 0);
  }

  length |= 0;

  if (position == null) {
    position = -1;
  } else {
    validatePosition(position, 'position', length);
  }

  if (length === 0) {
    return 0;
  }

  if (buffer.byteLength === 0) {
    throw new ERR_INVALID_ARG_VALUE('buffer', buffer,
                                    'is empty and cannot be written');
  }

  validateOffsetLengthRead(offset, length, buffer.byteLength);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.readSync(fd, buffer, offset, length, position);
    if (result !== undefined) return result;
  }

  return binding.read(fd, buffer, offset, length, position);
}

/**
 * Synchronously reads file from the
 * specified `fd` (file descriptor) and writes to an array
 * of `ArrayBufferView`s.
 * @param {number} fd
 * @param {ArrayBufferView[]} buffers
 * @param {number | null} [position]
 * @returns {number}
 */
function readvSync(fd, buffers, position) {
  validateBufferArray(buffers);

  if (typeof position !== 'number')
    position = null;

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.readvSync(fd, buffers, position);
    if (result !== undefined) return result;
  }

  return binding.readBuffers(fd, buffers, position);
}

/**
 * Synchronously writes `buffer` to the
 * specified `fd` (file descriptor).
 * @param {number} fd
 * @param {Buffer | TypedArray | DataView | string} buffer
 * @param {{
 *   offset?: number;
 *   length?: number;
 *   position?: number | null;
 *   }} [offsetOrOptions]
 * @param {number} [length]
 * @param {number} [position]
 * @returns {number}
 */
function writeSync(fd, buffer, offsetOrOptions, length, position) {
  const ctx = {};
  let result;

  let offset = offsetOrOptions;
  if (isArrayBufferView(buffer)) {
    if (typeof offset === 'object') {
      ({
        offset = 0,
        length = buffer.byteLength - offset,
        position = null,
      } = offsetOrOptions ?? kEmptyObject);
    }
    if (position === undefined)
      position = null;
    if (offset == null) {
      offset = 0;
    } else {
      validateInteger(offset, 'offset', 0);
    }
    if (typeof length !== 'number')
      length = buffer.byteLength - offset;
    validateOffsetLengthWrite(offset, length, buffer.byteLength);
    // V8 does not report -0 as an int32, which the binding requires.
    length |= 0;

    const h = vfsState.handlers;
    if (h !== null) {
      const vfsResult = h.writeSync(fd, buffer, offset, length, position);
      if (vfsResult !== undefined) return vfsResult;
    }

    result = binding.writeBuffer(fd, buffer, offset, length, position,
                                 undefined, ctx);
  } else {
    validateStringAfterArrayBufferView(buffer, 'buffer');
    validateEncoding(buffer, length);

    if (offset === undefined)
      offset = null;

    const h = vfsState.handlers;
    if (h !== null) {
      const bufAsBuffer = Buffer.from(buffer, length);
      const vfsResult = h.writeSync(fd, bufAsBuffer, 0, bufAsBuffer.length, offset);
      if (vfsResult !== undefined) return vfsResult;
    }

    result = binding.writeString(fd, buffer, offset, length,
                                 undefined, ctx);
  }
  handleErrorFromBinding(ctx);
  return result;
}

/**
 * Synchronously writes an array of `ArrayBufferView`s
 * to the specified `fd` (file descriptor).
 * @param {number} fd
 * @param {ArrayBufferView[]} buffers
 * @param {number | null} [position]
 * @returns {number}
 */
function writevSync(fd, buffers, position) {
  validateBufferArray(buffers);

  if (buffers.length === 0) {
    return 0;
  }

  if (typeof position !== 'number')
    position = null;

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.writevSync(fd, buffers, position);
    if (result !== undefined) return result;
  }

  return binding.writeBuffers(fd, buffers, position);
}

/**
 * Synchronously renames file at `oldPath` to
 * the pathname provided as `newPath`.
 * @param {string | Buffer | URL} oldPath
 * @param {string | Buffer | URL} newPath
 * @returns {void}
 */
function renameSync(oldPath, newPath) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.renameSync(oldPath, newPath);
    if (result !== undefined) return;
  }
  binding.rename(
    getValidatedPath(oldPath, 'oldPath'),
    getValidatedPath(newPath, 'newPath'),
  );
}

/**
 * Synchronously truncates the file.
 * @param {string | Buffer | URL} path
 * @param {number} [len]
 * @returns {void}
 */
function truncateSync(path, len) {
  if (len === undefined) {
    len = 0;
  }

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.truncateSync(path, len);
    if (result !== undefined) return;
  }

  // Allow error to be thrown, but still close fd.
  const fd = fs.openSync(path, 'r+');
  try {
    fs.ftruncateSync(fd, len);
  } finally {
    fs.closeSync(fd);
  }
}

/**
 * Synchronously truncates the file descriptor.
 * @param {number} fd
 * @param {number} [len]
 * @returns {void}
 */
function ftruncateSync(fd, len = 0) {
  validateInteger(len, 'len');

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.ftruncateSync(fd, len < 0 ? 0 : len);
    if (result !== undefined) return;
  }

  binding.ftruncate(fd, len < 0 ? 0 : len);
}

/**
 * Synchronously removes a directory.
 * @param {string | Buffer | URL} path
 * @param {object} [options]
 * @returns {void}
 */
function rmdirSync(path, options) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.rmdirSync(path);
    if (result !== undefined) return;
  }
  path = getValidatedPath(path);

  if (options?.recursive !== undefined) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.recursive',
      options.recursive,
      'is no longer supported',
    );
  }

  validateRmdirOptions(options);
  binding.rmdir(path);
}

/**
 * Synchronously removes files and
 * directories (modeled on the standard POSIX `rm` utility).
 * @param {string | Buffer | URL} path
 * @param {{
 *   force?: boolean;
 *   maxRetries?: number;
 *   recursive?: boolean;
 *   retryDelay?: number;
 *   }} [options]
 * @returns {void}
 */
function rmSync(path, options) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.rmSync(path, options);
    if (result !== undefined) return;
  }
  const opts = validateRmOptionsSync(path, options, false);
  return binding.rmSync(getValidatedPath(path), opts.maxRetries, opts.recursive, opts.retryDelay);
}

/**
 * Synchronously forces all currently queued I/O operations
 * associated with the file to the operating
 * system's synchronized I/O completion state.
 * @param {number} fd
 * @returns {void}
 */
function fdatasyncSync(fd) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.fdatasyncSync(fd);
    if (result !== undefined) return;
  }

  if (permission.isEnabled() && !permission.isAuditMode()) {
    throw new ERR_ACCESS_DENIED('fdatasync API is disabled when Permission Model is enabled.');
  }
  binding.fdatasync(fd);
}

/**
 * Synchronously requests for all data for the open
 * file descriptor to be flushed to the storage device.
 * @param {number} fd
 * @returns {void}
 */
function fsyncSync(fd) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.fsyncSync(fd);
    if (result !== undefined) return;
  }

  if (permission.isEnabled() && !permission.isAuditMode()) {
    throw new ERR_ACCESS_DENIED('fsync API is disabled when Permission Model is enabled.');
  }
  binding.fsync(fd);
}

/**
 * Synchronously creates a directory.
 * @param {string | Buffer | URL} path
 * @param {{
 *   recursive?: boolean;
 *   mode?: string | number;
 *   } | number} [options]
 * @returns {string | void}
 */
function mkdirSync(path, options) {
  const h = vfsState.handlers;
  if (h !== null) {
    const vfsResult = h.mkdirSync(path, options);
    if (vfsResult !== undefined) return vfsResult.result;
  }
  let mode = 0o777;
  let recursive = false;
  if (typeof options === 'number' || typeof options === 'string') {
    mode = parseFileMode(options, 'mode');
  } else if (options) {
    if (options.recursive !== undefined) {
      recursive = options.recursive;
      validateBoolean(recursive, 'options.recursive');
    }
    if (options.mode !== undefined) {
      mode = parseFileMode(options.mode, 'options.mode');
    }
  }

  const result = binding.mkdir(
    getValidatedPath(path),
    mode,
    recursive,
  );

  if (recursive) {
    return result;
  }
}

/**
 * Synchronously reads the entire contents of the `basePath` directory, see
 * `readdirRecursive`.
 * @param {string | Buffer} basePath
 * @param {{ encoding: string, withFileTypes: boolean }} options
 * @returns {string[] | Buffer[] | Dirent[]}
 */
function readdirSyncRecursive(basePath, options) {
  const withFileTypes = !!options.withFileTypes;
  if (permission.isEnabled()) {
    const context = {
      withFileTypes,
      results: [],
      dirs: [basePath],
      prefixes: [''],
    };
    for (let i = 0; i < context.dirs.length; i++) {
      const dir = context.dirs[i];
      const result = binding.readdir(dir, options.encoding, true);
      collectRecursiveReaddirResult(dir, context.prefixes[i], result, context);
    }
    return context.results;
  }
  const result = binding.readdirRecursive(basePath, options.encoding, withFileTypes);
  return result !== undefined && withFileTypes ? getRecursiveDirents(basePath, result) : result;
}

/**
 * Synchronously reads the contents of a directory.
 * @param {string | Buffer | URL} path
 * @param {string | {
 *   encoding?: string;
 *   withFileTypes?: boolean;
 *   recursive?: boolean;
 *   }} [options]
 * @returns {string | Buffer[] | Dirent[]}
 */
function readdirSync(path, options) {
  options = getOptions(options);
  path = getValidatedPath(path);
  if (options.recursive != null) {
    validateBoolean(options.recursive, 'options.recursive');
  }

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.readdirSync(path, options);
    if (result !== undefined) return result;
  }

  if (options.recursive) {
    return readdirSyncRecursive(path, options);
  }

  const result = binding.readdir(
    path,
    options.encoding,
    !!options.withFileTypes,
  );

  return result !== undefined && options.withFileTypes ? getDirents(path, result) : result;
}

/**
 * Synchronously retrieves the `fs.Stats` for
 * the file descriptor.
 * @param {number} fd
 * @param {{ bigint?: boolean; }} [options]
 * @returns {Stats | undefined}
 */
function fstatSync(fd, options = { __proto__: null, bigint: false }) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.fstatSync(fd);
    if (result !== undefined) return result;
  }
  const stats = binding.fstat(fd, options.bigint, undefined, false);
  if (stats === undefined) {
    return;
  }
  return getStatsFromBinding(stats);
}

/**
 * Synchronously retrieves the `fs.Stats` for
 * the symbolic link referred to by the `path`.
 * @param {string | Buffer | URL} path
 * @param {{
 *   bigint?: boolean;
 *   throwIfNoEntry?: boolean;
 *   }} [options]
 * @returns {Stats | undefined}
 */
function lstatSync(path, options = { __proto__: null, bigint: false, throwIfNoEntry: true }) {
  const h = vfsState.handlers;
  if (h !== null) {
    try {
      const result = h.lstatSync(path, options);
      if (result !== undefined) return result;
    } catch (err) {
      if (err?.code === 'ENOENT' && options?.throwIfNoEntry === false) return;
      throw err;
    }
  }
  path = getValidatedPath(path);
  if (permission.isEnabled() && !permission.has('fs.read', path) &&
      !permission.isAuditMode()) {
    const resource = BufferIsBuffer(path) ? BufferToString(path) : path;
    throw new ERR_ACCESS_DENIED('Access to this API has been restricted', 'FileSystemRead', resource);
  }
  const stats = binding.lstat(
    path,
    options.bigint,
    undefined,
    options.throwIfNoEntry,
  );

  if (stats === undefined) {
    return;
  }
  return getStatsFromBinding(stats);
}

/**
 * Synchronously retrieves the `fs.Stats`
 * for the `path`.
 * @param {string | Buffer | URL} path
 * @param {{
 *   bigint?: boolean;
 *   throwIfNoEntry?: boolean;
 *   }} [options]
 * @returns {Stats}
 */
function statSync(path, options = { __proto__: null, bigint: false, throwIfNoEntry: true }) {
  const h = vfsState.handlers;
  if (h !== null) {
    try {
      const result = h.statSync(path, options);
      if (result !== undefined) return result;
    } catch (err) {
      if (err?.code === 'ENOENT' && options?.throwIfNoEntry === false) return undefined;
      throw err;
    }
  }
  const stats = binding.stat(
    getValidatedPath(path),
    options.bigint,
    undefined,
    options.throwIfNoEntry,
  );
  if (stats === undefined) {
    return undefined;
  }
  return getStatsFromBinding(stats);
}

function statfsSync(path, options = { __proto__: null, bigint: false }) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.statfsSync(path, options);
    if (result !== undefined) return result;
  }

  const stats = binding.statfs(getValidatedPath(path), options.bigint);
  return getStatFsFromBinding(stats);
}

/**
 * Synchronously reads the contents of a symbolic link
 * referred to by `path`.
 * @param {string | Buffer | URL} path
 * @param {{ encoding?: string; } | string} [options]
 * @returns {string | Buffer}
 */
function readlinkSync(path, options) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.readlinkSync(path, options);
    if (result !== undefined) return result;
  }
  options = getOptions(options);
  return binding.readlink(getValidatedPath(path), options.encoding);
}

/**
 * Synchronously creates the link called `path`
 * pointing to `target`.
 * @param {string | Buffer | URL} target
 * @param {string | Buffer | URL} path
 * @param {string | null} [type]
 * @returns {void}
 */
function symlinkSync(target, path, type) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.symlinkSync(target, path, type);
    if (result !== undefined) return;
  }
  validateOneOf(type, 'type', ['dir', 'file', 'junction', null, undefined]);
  if (isWindows && type == null) {
    const absoluteTarget = pathModule.resolve(`${path}`, '..', `${target}`);
    if (statSync(absoluteTarget, { throwIfNoEntry: false })?.isDirectory()) {
      type = 'dir';
    }
  }

  // Due to the nature of Node.js runtime, symlinks has different edge cases that can bypass
  // the permission model security guarantees. Thus, this API is disabled unless fs.read
  // and fs.write permission has been given.
  if (permission.isEnabled() && !permission.has('fs') &&
      !permission.isAuditMode()) {
    throw new ERR_ACCESS_DENIED('fs.symlink API requires full fs.read and fs.write permissions.');
  }

  target = getValidatedPath(target, 'target');
  path = getValidatedPath(path);

  binding.symlink(
    preprocessSymlinkDestination(target, type, path),
    path,
    stringToSymlinkType(type),
  );
}

/**
 * Synchronously creates a new link from the `existingPath`
 * to the `newPath`.
 * @param {string | Buffer | URL} existingPath
 * @param {string | Buffer | URL} newPath
 * @returns {void}
 */
function linkSync(existingPath, newPath) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.linkSync(existingPath, newPath);
    if (result !== undefined) return;
  }

  existingPath = getValidatedPath(existingPath, 'existingPath');
  newPath = getValidatedPath(newPath, 'newPath');

  binding.link(
    existingPath,
    newPath,
  );
}

/**
 * Synchronously removes a file or symbolic link.
 * @param {string | Buffer | URL} path
 * @returns {void}
 */
function unlinkSync(path) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.unlinkSync(path);
    if (result !== undefined) return;
  }
  binding.unlink(getValidatedPath(path));
}

/**
 * Synchronously sets the permissions on the file.
 * @param {number} fd
 * @param {string | number} mode
 * @returns {void}
 */
function fchmodSync(fd, mode) {
  mode = parseFileMode(mode, 'mode');

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.fchmodSync(fd, mode);
    if (result !== undefined) return;
  }

  if (permission.isEnabled() && !permission.isAuditMode()) {
    throw new ERR_ACCESS_DENIED('fchmod API is disabled when Permission Model is enabled.');
  }
  binding.fchmod(fd, mode);
}

/**
 * Synchronously changes the permissions on a symbolic link.
 * @param {string | Buffer | URL} path
 * @param {number} mode
 * @returns {void}
 */
function lchmodSync(path, mode) {
  const fd = fs.openSync(path, O_WRONLY | O_SYMLINK);

  // Prefer to return the chmod error, if one occurs,
  // but still try to close, and report closing errors if they occur.
  try {
    fs.fchmodSync(fd, mode);
  } finally {
    fs.closeSync(fd);
  }
}

/**
 * Synchronously changes the permissions of a file.
 * @param {string | Buffer | URL} path
 * @param {string | number} mode
 * @returns {void}
 */
function chmodSync(path, mode) {
  path = getValidatedPath(path);
  mode = parseFileMode(mode, 'mode');

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.chmodSync(path, mode);
    if (result !== undefined) return;
  }

  binding.chmod(path, mode);
}

/**
 * Synchronously sets the owner of the symbolic link.
 * @param {string | Buffer | URL} path
 * @param {number} uid
 * @param {number} gid
 * @returns {void}
 */
function lchownSync(path, uid, gid) {
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.lchownSync(path, uid, gid);
    if (result !== undefined) return;
  }

  binding.lchown(path, uid, gid);
}

/**
 * Synchronously sets the owner of the file.
 * @param {number} fd
 * @param {number} uid
 * @param {number} gid
 * @returns {void}
 */
function fchownSync(fd, uid, gid) {
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.fchownSync(fd);
    if (result !== undefined) return;
  }

  if (permission.isEnabled() && !permission.isAuditMode()) {
    throw new ERR_ACCESS_DENIED('fchown API is disabled when Permission Model is enabled.');
  }

  binding.fchown(fd, uid, gid);
}

/**
 * Synchronously changes the owner and group
 * of a file.
 * @param {string | Buffer | URL} path
 * @param {number} uid
 * @param {number} gid
 * @returns {void}
 */
function chownSync(path, uid, gid) {
  path = getValidatedPath(path);
  validateInteger(uid, 'uid', -1, kMaxUserId);
  validateInteger(gid, 'gid', -1, kMaxUserId);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.chownSync(path, uid, gid);
    if (result !== undefined) return;
  }

  binding.chown(path, uid, gid);
}

/**
 * Synchronously changes the file system timestamps
 * of the object referenced by `path`.
 * @param {string | Buffer | URL} path
 * @param {number | string | Date} atime
 * @param {number | string | Date} mtime
 * @returns {void}
 */
function utimesSync(path, atime, mtime) {
  path = getValidatedPath(path);
  atime = toUnixTimestamp(atime);
  mtime = toUnixTimestamp(mtime);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.utimesSync(path, atime, mtime);
    if (result !== undefined) return;
  }

  binding.utimes(path, atime, mtime);
}

/**
 * Synchronously changes the file system timestamps
 * of the object referenced by the
 * supplied `fd` (file descriptor).
 * @param {number} fd
 * @param {number | string | Date} atime
 * @param {number | string | Date} mtime
 * @returns {void}
 */
function futimesSync(fd, atime, mtime) {
  atime = toUnixTimestamp(atime, 'atime');
  mtime = toUnixTimestamp(mtime, 'mtime');

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.futimesSync(fd, atime, mtime);
    if (result !== undefined) return;
  }

  if (permission.isEnabled() && !permission.isAuditMode()) {
    throw new ERR_ACCESS_DENIED('futimes API is disabled when Permission Model is enabled.');
  }

  binding.futimes(fd, atime, mtime);
}

/**
 * Synchronously changes the access and modification
 * times of a file in the same way as `fs.utimesSync()`.
 * @param {string | Buffer | URL} path
 * @param {number | string | Date} atime
 * @param {number | string | Date} mtime
 * @returns {void}
 */
function lutimesSync(path, atime, mtime) {
  path = getValidatedPath(path);
  atime = toUnixTimestamp(atime);
  mtime = toUnixTimestamp(mtime);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.lutimesSync(path, atime, mtime);
    if (result !== undefined) return;
  }

  binding.lutimes(path, atime, mtime);
}

/**
 * Synchronously writes data to the file.
 * @param {string | Buffer | URL | number} path
 * @param {string | Buffer | TypedArray | DataView} data
 * @param {{
 *   encoding?: string | null;
 *   mode?: number;
 *   flag?: string;
 *   flush?: boolean;
 *   } | string} [options]
 * @returns {void}
 */
function writeFileSync(path, data, options) {
  options = getOptions(options, {
    encoding: 'utf8',
    mode: 0o666,
    flag: 'w',
    flush: false,
  });
  const flush = options.flush ?? false;
  validateBoolean(flush, 'options.flush');
  parseFileMode(options.mode, 'mode', 0o666);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.writeFileSync(path, data, options);
    if (result !== undefined) return;
  }

  const flag = options.flag || 'w';

  // C++ fast path for string data and UTF8 encoding
  if (typeof data === 'string' && (options.encoding === 'utf8' || options.encoding === 'utf-8')) {
    if (isInt32(path)) {
      // V8 does not report -0 as an int32, so it would reach the binding as a
      // path instead of a file descriptor.
      path |= 0;
    } else {
      path = getValidatedPath(path);
    }

    return binding.writeFileUtf8(
      path,
      data,
      stringToFlags(flag),
      parseFileMode(options.mode, 'mode', 0o666),
    );
  }

  if (!isArrayBufferView(data)) {
    validateStringAfterArrayBufferView(data, 'data');
    data = Buffer.from(data, options.encoding || 'utf8');
  }

  const isUserFd = isFd(path); // File descriptor ownership
  const fd = isUserFd ? path : fs.openSync(path, flag, options.mode);

  let offset = 0;
  let length = data.byteLength;
  try {
    while (length > 0) {
      const written = fs.writeSync(fd, data, offset, length);
      offset += written;
      length -= written;
    }

    if (flush) {
      fs.fsyncSync(fd);
    }
  } finally {
    if (!isUserFd) fs.closeSync(fd);
  }
}

/**
 * Synchronously appends data to a file.
 * @param {string | Buffer | URL | number} path
 * @param {string | Buffer} data
 * @param {{
 *   encoding?: string | null;
 *   mode?: number;
 *   flag?: string;
 *   } | string} [options]
 * @returns {void}
 */
function appendFileSync(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'a' });
  parseFileMode(options.mode, 'mode', 0o666);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.appendFileSync(path, data, options);
    if (result !== undefined) return;
  }

  // Don't make changes directly on options object
  options = copyObject(options);

  // Force append behavior when using a supplied file descriptor
  if (!options.flag || isFd(path))
    options.flag = 'a';

  fs.writeFileSync(path, data, options);
}

/**
 * Returns the resolved pathname.
 * @param {string | Buffer | URL} p
 * @param {string | { encoding?: string | null; }} [options]
 * @returns {string | Buffer}
 */
function realpathSync(p, options) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.realpathSync(p, options);
    if (result !== undefined) return result;
  }
  options = getOptions(options);
  p = toPathIfFileURL(p);
  if (typeof p !== 'string') {
    p += '';
  }
  validatePath(p);
  p = toRealpathInput(p);

  // Check the resolved input path up front so that the lstat walk below cannot
  // be used to probe the existence of paths outside the granted set.
  if (permission.isEnabled() && !permission.has('fs.read', p) &&
      !permission.isAuditMode()) {
    throw new ERR_ACCESS_DENIED(
      'Access to this API has been restricted. Use --allow-fs-read to manage permissions.',
      'FileSystemRead',
      p,
    );
  }

  const cache = options[realpathCacheKey];
  const original = p;
  const maybeCachedResult = cache?.get(original);
  if (maybeCachedResult) {
    return maybeCachedResult;
  }

  const seenLinks = new SafeMap();
  const knownHard = new SafeSet();
  // Whether the symlink this walk resolved last pointed at a pipe or a
  // socket, which is where the walk stops. It cannot be read back from the
  // shared stat buffer, which holds the last stat made anywhere in the
  // process rather than the last one made here.
  let reachedPipeOrSocket = false;

  // Current character position in p
  let pos;
  // The partial path so far, including a trailing slash if any
  let current;
  // The partial path without a trailing slash (except when pointing at a root)
  let base;
  // The partial path scanned in the previous round, with slash
  let previous;

  // Skip over roots
  current = base = splitRoot(p);
  pos = current.length;

  // On windows, check that the root exists. On unix there is no need.
  if (isWindows) {
    const out = binding.lstat(
      getRealpathRootLstatPath(base), false, undefined, true /* throwIfNoEntry */);
    if (out === undefined) {
      return;
    }
    knownHard.add(base);
  }

  // Walk down the path, swapping out linked path parts for their real
  // values
  // NB: p.length changes.
  while (pos < p.length) {
    // find the next part
    const result = nextPart(p, pos);
    const part = result === -1 ?
      StringPrototypeSlice(p, pos) :
      StringPrototypeSlice(p, pos, result);
    previous = current;
    if (result === -1) {
      const last = StringPrototypeSlice(p, pos);
      current += last;
      base = previous + last;
      pos = p.length;
    } else {
      current += StringPrototypeSlice(p, pos, result + 1);
      base = previous + StringPrototypeSlice(p, pos, result);
      pos = result + 1;
    }

    if (part === '' || part === '.') {
      current = previous;
      continue;
    }
    if (part === '..') {
      base = pathModule.dirname(previous);
      current = base;
      if (current[current.length - 1] !== CHAR_FORWARD_SLASH &&
          current[current.length - 1] !== CHAR_BACKWARD_SLASH) {
        current += pathModule.sep;
      }
      continue;
    }
    // Continue if not a symlink, break if a pipe/socket
    if (knownHard.has(base) || cache?.get(base) === base) {
      if (reachedPipeOrSocket) {
        break;
      }
      continue;
    }

    let resolvedLink;
    const maybeCachedResolved = cache?.get(base);
    if (maybeCachedResolved) {
      resolvedLink = maybeCachedResolved;
    } else {
      // Use stats array directly to avoid creating an fs.Stats instance just
      // for our internal use.

      const stats = binding.lstat(base, true, undefined, true /* throwIfNoEntry */);
      if (stats === undefined) {
        return;
      }

      if (!isFileType(stats, S_IFLNK)) {
        knownHard.add(base);
        cache?.set(base, base);
        continue;
      }

      // Read the link if it wasn't read before
      // dev/ino always return 0 on windows, so skip the check.
      let linkTarget = null;
      let id;
      if (!isWindows) {
        const dev = BigIntPrototypeToString(stats[0], 32);
        const ino = BigIntPrototypeToString(stats[7], 32);
        id = `${dev}:${ino}`;
        if (seenLinks.has(id)) {
          linkTarget = seenLinks.get(id);
        }
      }
      if (linkTarget === null) {
        const targetStats = binding.stat(base, false, undefined, true);
        reachedPipeOrSocket = isFileType(targetStats, S_IFIFO) ||
          isFileType(targetStats, S_IFSOCK);
        linkTarget = binding.readlink(base, undefined);
      }
      resolvedLink = joinRealpathLink(previous, linkTarget);

      // On POSIX, a target containing `..` resolves relative to the path
      // walked before it, so it cannot be cached by the link alone.
      if (isWindows || StringPrototypeIndexOf(linkTarget, '..') === -1) {
        cache?.set(base, pathModule.resolve(resolvedLink));
      }
      if (!isWindows) seenLinks.set(id, linkTarget);
    }

    // Resolve the link, then start over
    p = joinRealpathRemainder(resolvedLink, StringPrototypeSlice(p, pos));

    // Skip over roots
    current = base = splitRoot(p);
    pos = current.length;

    // On windows, check that the root exists. On unix there is no need.
    if (isWindows && !knownHard.has(base)) {
      const out = binding.lstat(
        getRealpathRootLstatPath(base), false, undefined, true /* throwIfNoEntry */);
      if (out === undefined) {
        return;
      }
      knownHard.add(base);
    }
  }

  p = pathModule.resolve(p);
  cache?.set(original, p);
  return encodeRealpathResult(p, options);
}

/**
 * Returns the resolved pathname.
 * @param {string | Buffer | URL} path
 * @param {string | { encoding?: string; }} [options]
 * @returns {string | Buffer}
 */
realpathSync.native = (path, options) => {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.realpathSync(path, options);
    if (result !== undefined) return result;
  }
  options = getOptions(options);
  return binding.realpath(
    getValidatedPath(path),
    options.encoding,
  );
};

/**
 * Synchronously creates a unique temporary directory.
 * @param {string | Buffer | URL} prefix
 * @param {string | { encoding?: string; }} [options]
 * @returns {string}
 */
function mkdtempSync(prefix, options) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.mkdtempSync(prefix, options);
    if (result !== undefined) return result;
  }

  options = getOptions(options);
  if (BufferIsBuffer(prefix)) {
    options = { ...options, encoding: 'buffer' };
  }
  prefix = getValidatedPath(prefix, 'prefix');
  warnOnNonPortableTemplate(prefix);
  return binding.mkdtemp(prefix, options.encoding);
}

/**
 * Synchronously creates a unique temporary directory.
 * The returned value is a disposable object which removes the
 * directory and its contents when disposed.
 * @param {string | Buffer | URL} prefix
 * @param {string | { encoding?: string; }} [options]
 * @returns {object} A disposable object with a "path" property.
 */
function mkdtempDisposableSync(prefix, options) {
  options = getOptions(options);
  if (BufferIsBuffer(prefix)) {
    options = { ...options, encoding: 'buffer' };
  }
  prefix = getValidatedPath(prefix, 'prefix');
  warnOnNonPortableTemplate(prefix);

  const path = binding.mkdtemp(prefix, options.encoding);
  // Stash the full path in case of process.chdir()
  const fullPath = pathModule.resolve(process.cwd(), path);

  const remove = () => {
    binding.rmSync(fullPath, 0 /* maxRetries */, true /* recursive */, 100 /* retryDelay */);
  };
  return {
    path,
    remove,
    [SymbolDispose]() {
      remove();
    },
  };
}

/**
 * Synchronously creates and opens a unique temporary file.
 * @param {string | Buffer | URL} prefix
 * @param {string | { encoding?: string; }} [options]
 * @returns {{ path: string | Buffer, fd: number }}
 */
function mkstempSync(prefix, options) {
  options = getOptions(options);
  if (BufferIsBuffer(prefix)) {
    options = { ...options, encoding: 'buffer' };
  }
  prefix = getValidatedPath(prefix, 'prefix');
  warnOnNonPortableTemplate(prefix);

  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.mkstempSync(prefix, options);
    if (result !== undefined) return result;
  }

  const result = binding.mkstemp(prefix, options.encoding);
  return { path: result[0], fd: result[1] };
}

/**
 * Synchronously copies `src` to `dest`. By
 * default, `dest` is overwritten if it already exists.
 * @param {string | Buffer | URL} src
 * @param {string | Buffer | URL} dest
 * @param {number} [mode]
 * @returns {void}
 */
function copyFileSync(src, dest, mode) {
  const h = vfsState.handlers;
  if (h !== null) {
    const result = h.copyFileSync(src, dest, mode);
    if (result !== undefined) return;
  }
  binding.copyFile(
    getValidatedPath(src, 'src'),
    getValidatedPath(dest, 'dest'),
    mode,
  );
}

/**
 * Synchronously copies `src` to `dest`. `src` can be a file, directory, or
 * symlink. The contents of directories will be copied recursively.
 * @param {string | URL} src
 * @param {string | URL} dest
 * @param {object} [options]
 * @returns {void}
 */
function cpSync(src, dest, options) {
  options = validateCpOptions(options);
  src = getValidatedPath(src, 'src');
  dest = getValidatedPath(dest, 'dest');
  lazyLoadCp()(src, dest, options);
}

function globSync(pattern, options) {
  const Glob = lazyGlob();
  return new Glob(pattern, options).globSync();
}

const synchronousExports = {
  appendFile: appendFileSync,
  access: accessSync,
  chown: chownSync,
  chmod: chmodSync,
  close: closeSync,
  copyFile: copyFileSync,
  cp: cpSync,
  exists: existsSync,
  fchown: fchownSync,
  fchmod: fchmodSync,
  fdatasync: fdatasyncSync,
  fstat: fstatSync,
  fsync: fsyncSync,
  ftruncate: ftruncateSync,
  futimes: futimesSync,
  glob: globSync,
  lchown: lchownSync,
  lchmod: constants.O_SYMLINK !== undefined ? lchmodSync : undefined,
  link: linkSync,
  lstat: lstatSync,
  lutimes: lutimesSync,
  mkdir: mkdirSync,
  mkdtemp: mkdtempSync,
  mkdtempDisposable: mkdtempDisposableSync,
  mkstemp: mkstempSync,
  open: openSync,
  openAsBlob: openAsBlobSync,
  readdir: readdirSync,
  read: readSync,
  readv: readvSync,
  readFile: readFileSync,
  readlink: readlinkSync,
  realpath: realpathSync,
  rename: renameSync,
  rm: rmSync,
  rmdir: rmdirSync,
  stat: statSync,
  statfs: statfsSync,
  symlink: symlinkSync,
  truncate: truncateSync,
  unlink: unlinkSync,
  utimes: utimesSync,
  writeFile: writeFileSync,
  write: writeSync,
  writev: writevSync,

  constants,
};

ObjectDefineProperty(synchronousExports, 'opendir', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    return require('internal/fs/dir').opendirSync;
  },
});

// This object exists to be used when one synchronous method calls another.
// As long as we preserve legacy monkey-patching behaviour, we need to take
// them from `fs` object. When `lib/fs.js` is imported, it replaces this
// object with the public userland-facing `fs`.
fs = {
  __proto__: null,
  get closeSync() {
    return synchronousExports.close;
  },
  get fchmodSync() {
    return synchronousExports.fchmod;
  },
  get ftruncateSync() {
    return synchronousExports.ftruncate;
  },
  get fsyncSync() {
    return synchronousExports.fsync;
  },
  get openSync() {
    return synchronousExports.open;
  },
  get readSync() {
    return synchronousExports.read;
  },
  get writeFileSync() {
    return synchronousExports.writeFile;
  },
  get writeSync() {
    return synchronousExports.write;
  },
};

module.exports = {
  exports: synchronousExports,
  setFS,
};
