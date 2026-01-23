'use strict';

const {
  ArrayFrom,
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypeMap,
  ArrayPrototypeUnshift,
  Date,
  DateNow,
  FunctionPrototypeCall,
  MapPrototypeForEach,
  MathCeil,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototype,
  ObjectPrototypeHasOwnProperty,
  SafeMap,
  SafeSet,
  SetPrototypeForEach,
  String,
  StringPrototypeEndsWith,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
  SymbolDispose,
  uncurryThis,
} = primordials;

const {
  UVException,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const {
  validateBoolean,
  validateObject,
  validateStringArray,
} = require('internal/validators');

const { kEmptyObject, emitExperimentalWarning } = require('internal/util');
const { isUint8Array } = require('internal/util/types');
const { Buffer } = require('buffer');
const { fileURLToPath, isURL } = require('internal/url');

const BufferPrototypeToString = uncurryThis(Buffer.prototype.toString);
const BufferFrom = Buffer.from;
const BufferConcat = Buffer.concat;
const BufferAlloc = Buffer.alloc;
const { UV_ENOENT, UV_ENOTDIR, UV_ENOTEMPTY, UV_EEXIST, UV_EISDIR } =
  internalBinding('uv');

const fs = require('fs');
const fsPromises = require('fs/promises');
const {
  join: pathJoin,
  resolve: pathResolve,
  dirname: pathDirname,
  sep: pathSep,
} = require('path');
const {
  Stats,
} = require('internal/fs/utils');

/**
 * @enum {('readFile'|'writeFile'|'appendFile'|'stat'|'lstat'|'access'|
 *   'exists'|'unlink'|'mkdir'|'rmdir'|'readdir')[]} Supported fs APIs
 */
const SUPPORTED_APIS = [
  'readFile',
  'writeFile',
  'appendFile',
  'stat',
  'lstat',
  'access',
  'exists',
  'unlink',
  'mkdir',
  'rmdir',
  'readdir',
];

// Path segments that could lead to prototype pollution attacks.
const kDangerousPathSegments = new SafeSet(['__proto__', 'constructor', 'prototype']);

/**
 * @typedef {object} MockFileSystemOptions
 * @property {{[path: string]: string|Buffer|Uint8Array}} [files] Virtual files to create.
 * @property {boolean} [isolate] If true, block access to real file system.
 * @property {SUPPORTED_APIS} [apis] Which fs APIs to mock.
 */

// File mode constants from POSIX standard.
const S_IFDIR = 0o40000; // Directory.
const S_IFREG = 0o100000; // Regular file.

// Standard block sizes for Unix-like systems.
const DEFAULT_BLKSIZE = 4096; // Default filesystem block size (4 KiB).
const BLOCK_SIZE = 512; // Standard Unix block size for st_blocks.

/**
 * Creates a mock Stats object that properly inherits from fs.Stats.prototype.
 * This ensures that `stats instanceof fs.Stats` returns true.
 * @param {{isDirectory?: boolean, size?: number, mode?: number}} options
 * @returns {fs.Stats}
 */
function createMockStats({ isDirectory = false, size = 0, mode = 0o644 }) {
  const nowMs = DateNow();
  const nowDate = new Date(nowMs);

  // Create object that inherits from Stats.prototype so instanceof checks work.
  // We use Object.create to avoid calling the deprecated Stats constructor.
  return {
    __proto__: Stats.prototype,
    dev: 0,
    ino: 0,
    mode: isDirectory ? S_IFDIR | mode : S_IFREG | mode,
    nlink: 1,
    uid: 0,
    gid: 0,
    rdev: 0,
    size,
    blksize: DEFAULT_BLKSIZE,
    blocks: MathCeil(size / BLOCK_SIZE),
    atimeMs: nowMs,
    mtimeMs: nowMs,
    ctimeMs: nowMs,
    birthtimeMs: nowMs,
    atime: nowDate,
    mtime: nowDate,
    ctime: nowDate,
    birthtime: nowDate,
  };
}

/**
 * @param {string} syscall
 * @param {string} filepath
 * @returns {Error}
 */
function createENOENT(syscall, filepath) {
  return new UVException({
    __proto__: null,
    errno: UV_ENOENT,
    syscall,
    path: filepath,
    message: 'no such file or directory',
  });
}

/**
 * @param {string} syscall
 * @param {string} filepath
 * @returns {Error}
 */
function createENOTDIR(syscall, filepath) {
  return new UVException({
    __proto__: null,
    errno: UV_ENOTDIR,
    syscall,
    path: filepath,
    message: 'not a directory',
  });
}

/**
 * @param {string} syscall
 * @param {string} filepath
 * @returns {Error}
 */
function createENOTEMPTY(syscall, filepath) {
  return new UVException({
    __proto__: null,
    errno: UV_ENOTEMPTY,
    syscall,
    path: filepath,
    message: 'directory not empty',
  });
}

/**
 * @param {string} syscall
 * @param {string} filepath
 * @returns {Error}
 */
function createEEXIST(syscall, filepath) {
  return new UVException({
    __proto__: null,
    errno: UV_EEXIST,
    syscall,
    path: filepath,
    message: 'file already exists',
  });
}

/**
 * @param {string} syscall
 * @param {string} filepath
 * @returns {Error}
 */
function createEISDIR(syscall, filepath) {
  return new UVException({
    __proto__: null,
    errno: UV_EISDIR,
    syscall,
    path: filepath,
    message: 'illegal operation on a directory',
  });
}

/**
 * @param {string} str
 * @returns {string}
 */
function getFirstPathSegment(str) {
  const sepIndex = StringPrototypeIndexOf(str, pathSep);
  if (sepIndex === -1) {
    return str;
  }
  return StringPrototypeSlice(str, 0, sepIndex);
}

/**
 * Gets a property descriptor and validates it can be mocked.
 * Throws if the property is non-configurable.
 * @param {object} obj The object containing the property.
 * @param {string} prop The property name.
 * @param {string} objName The object name for error messages.
 * @returns {PropertyDescriptor} The property descriptor.
 */
function getAndValidateDescriptor(obj, prop, objName) {
  const descriptor = ObjectGetOwnPropertyDescriptor(obj, prop);
  if (descriptor && !descriptor.configurable) {
    throw new ERR_INVALID_STATE(
      `Cannot mock ${objName}.${prop} because it is non-configurable`,
    );
  }
  return descriptor;
}

/**
 * Defines a mocked property on an object, preserving the original descriptor shape.
 * @param {object} obj The object to define the property on.
 * @param {string} prop The property name.
 * @param {Function} value The mocked function value.
 * @param {PropertyDescriptor} originalDescriptor The original descriptor to base the new one on.
 */
function defineMockedProperty(obj, prop, value, originalDescriptor) {
  ObjectDefineProperty(obj, prop, {
    __proto__: null,
    value,
    writable: originalDescriptor?.writable ?? true,
    enumerable: originalDescriptor?.enumerable ?? true,
    configurable: true, // Must be configurable to allow restoration.
  });
}

class MockFileSystem {
  #isEnabled = false;
  #files = new SafeMap(); // Normalized path -> { content: Buffer, stat: {...} }.
  #directories = new SafeSet(); // Normalized paths of virtual directories.
  #isolate = false;
  #apisInContext = [];

  // Original method descriptors for restoration.
  #originals = { __proto__: null };

  #normalizePath(filepath) {
    if (typeof filepath === 'string') {
      return pathResolve(filepath);
    }
    if (isUint8Array(filepath)) {
      const bufferPath = BufferFrom(filepath);
      return pathResolve(
        BufferPrototypeToString(bufferPath, 'utf8'),
      );
    }
    if (isURL(filepath)) {
      if (filepath.protocol !== 'file:') {
        throw new ERR_INVALID_ARG_VALUE('path', filepath, 'must be a file URL');
      }
      return fileURLToPath(filepath);
    }
    return pathResolve(String(filepath));
  }

  #virtualExists(normalizedPath) {
    return (
      this.#files.has(normalizedPath) || this.#directories.has(normalizedPath)
    );
  }

  #populateFiles(files) {
    // Check if __proto__ was used in the object literal (which modifies prototype).
    if (ObjectGetPrototypeOf(files) !== ObjectPrototype) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.files',
        '__proto__',
        'cannot use __proto__ as a key in the files object',
      );
    }

    const filePaths = ObjectKeys(files);
    for (let i = 0; i < filePaths.length; i++) {
      const filepath = filePaths[i];

      // Ensure the property is own (not inherited).
      if (!ObjectPrototypeHasOwnProperty(files, filepath)) {
        continue;
      }

      // Check for dangerous path segments that could lead to prototype pollution.
      const segments = StringPrototypeSplit(filepath, pathSep);
      for (let j = 0; j < segments.length; j++) {
        if (kDangerousPathSegments.has(segments[j])) {
          throw new ERR_INVALID_ARG_VALUE(
            'options.files',
            filepath,
            'cannot contain __proto__, constructor, or prototype in path',
          );
        }
      }

      const content = files[filepath];
      const normalizedPath = this.#normalizePath(filepath);

      // Convert content to Buffer.
      let buffer;
      if (typeof content === 'string') {
        buffer = BufferFrom(content, 'utf8');
      } else if (isUint8Array(content)) {
        buffer = BufferFrom(content);
      } else {
        throw new ERR_INVALID_ARG_TYPE(
          `options.files['${filepath}']`,
          ['string', 'Buffer', 'Uint8Array'],
          content,
        );
      }

      // Store the file.
      this.#files.set(normalizedPath, {
        __proto__: null,
        content: buffer,
        stat: createMockStats({ __proto__: null, size: buffer.length }),
      });

      // Ensure parent directories exist in the virtual fs.
      let dir = pathDirname(normalizedPath);
      while (dir !== pathDirname(dir)) {
        this.#directories.add(dir);
        dir = pathDirname(dir);
      }
    }
  }

  #toggleEnableApis(activate) {
    const self = this;

    const options = {
      __proto__: null,
      toFake: {
        '__proto__': null,
        readFile() {
          this.#originals.readFile = getAndValidateDescriptor(fs, 'readFile', 'fs');
          this.#originals.readFileSync = getAndValidateDescriptor(fs, 'readFileSync', 'fs');
          this.#originals.promisesReadFile = getAndValidateDescriptor(fsPromises, 'readFile', 'fs/promises');

          const origReadFileSync = this.#originals.readFileSync.value;
          const origReadFile = this.#originals.readFile.value;
          const origPromisesReadFile = this.#originals.promisesReadFile.value;

          const mockedReadFileSync = function readFileSync(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const file = self.#files.get(normalizedPath);

            if (file) {
              const encoding = typeof opts === 'string' ? opts : opts?.encoding;
              if (encoding) {
                return BufferPrototypeToString(file.content, encoding);
              }
              return BufferFrom(file.content);
            }

            if (self.#directories.has(normalizedPath)) {
              throw createEISDIR('open', normalizedPath);
            }

            if (self.#isolate) {
              throw createENOENT('open', normalizedPath);
            }

            return origReadFileSync(filepath, opts);
          };
          defineMockedProperty(fs, 'readFileSync', mockedReadFileSync, this.#originals.readFileSync);

          const mockedReadFile = function readFile(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#files.has(normalizedPath) || self.#directories.has(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                const result = mockedReadFileSync(filepath, opts);
                process.nextTick(callback, null, result);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origReadFile(filepath, opts, callback);
            }
          };
          defineMockedProperty(fs, 'readFile', mockedReadFile, this.#originals.readFile);

          const mockedPromisesReadFile = async function readFile(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#files.has(normalizedPath) || self.#directories.has(normalizedPath);

            if (isVirtual || self.#isolate) {
              return mockedReadFileSync(filepath, opts);
            }

            return origPromisesReadFile(filepath, opts);
          };
          defineMockedProperty(fsPromises, 'readFile', mockedPromisesReadFile, this.#originals.promisesReadFile);
        },

        writeFile() {
          this.#originals.writeFile = getAndValidateDescriptor(fs, 'writeFile', 'fs');
          this.#originals.writeFileSync = getAndValidateDescriptor(fs, 'writeFileSync', 'fs');
          this.#originals.promisesWriteFile = getAndValidateDescriptor(fsPromises, 'writeFile', 'fs/promises');

          const mockedWriteFileSync = function writeFileSync(filepath, data, opts) {
            const normalizedPath = self.#normalizePath(filepath);

            if (self.#directories.has(normalizedPath)) {
              throw createEISDIR('open', normalizedPath);
            }

            // Always write to virtual fs when mock is enabled.
            // This keeps test environments isolated from the real filesystem.
            let buffer;
            if (typeof data === 'string') {
              const encoding = typeof opts === 'string' ? opts : opts?.encoding || 'utf8';
              buffer = BufferFrom(data, encoding);
            } else if (isUint8Array(data)) {
              buffer = BufferFrom(data);
            } else {
              buffer = BufferFrom(String(data));
            }

            self.#files.set(normalizedPath, {
              __proto__: null,
              content: buffer,
              stat: createMockStats({ __proto__: null, size: buffer.length }),
            });

            let dir = pathDirname(normalizedPath);
            while (dir !== pathDirname(dir)) {
              self.#directories.add(dir);
              dir = pathDirname(dir);
            }
          };
          defineMockedProperty(fs, 'writeFileSync', mockedWriteFileSync, this.#originals.writeFileSync);

          const mockedWriteFile = function writeFile(filepath, data, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            // Always write to virtual fs when mock is enabled.
            try {
              mockedWriteFileSync(filepath, data, opts);
              process.nextTick(callback, null);
            } catch (err) {
              process.nextTick(callback, err);
            }
          };
          defineMockedProperty(fs, 'writeFile', mockedWriteFile, this.#originals.writeFile);

          const mockedPromisesWriteFile = async function writeFile(filepath, data, opts) {
            // Always write to virtual fs when mock is enabled.
            mockedWriteFileSync(filepath, data, opts);
          };
          defineMockedProperty(fsPromises, 'writeFile', mockedPromisesWriteFile, this.#originals.promisesWriteFile);
        },

        appendFile() {
          this.#originals.appendFile = getAndValidateDescriptor(fs, 'appendFile', 'fs');
          this.#originals.appendFileSync = getAndValidateDescriptor(fs, 'appendFileSync', 'fs');
          this.#originals.promisesAppendFile = getAndValidateDescriptor(fsPromises, 'appendFile', 'fs/promises');

          const mockedAppendFileSync = function appendFileSync(filepath, data, opts) {
            const normalizedPath = self.#normalizePath(filepath);

            if (self.#directories.has(normalizedPath)) {
              throw createEISDIR('open', normalizedPath);
            }

            const existingFile = self.#files.get(normalizedPath);

            // Always append to virtual fs when mock is enabled.
            let buffer;
            if (typeof data === 'string') {
              const encoding = typeof opts === 'string' ? opts : opts?.encoding || 'utf8';
              buffer = BufferFrom(data, encoding);
            } else if (isUint8Array(data)) {
              buffer = BufferFrom(data);
            } else {
              buffer = BufferFrom(String(data));
            }

            const existingContent = existingFile ? existingFile.content : BufferAlloc(0);
            const newContent = BufferConcat([existingContent, buffer]);

            self.#files.set(normalizedPath, {
              __proto__: null,
              content: newContent,
              stat: createMockStats({ __proto__: null, size: newContent.length }),
            });

            let dir = pathDirname(normalizedPath);
            while (dir !== pathDirname(dir)) {
              self.#directories.add(dir);
              dir = pathDirname(dir);
            }
          };
          defineMockedProperty(fs, 'appendFileSync', mockedAppendFileSync, this.#originals.appendFileSync);

          const mockedAppendFile = function appendFile(filepath, data, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            // Always append to virtual fs when mock is enabled.
            try {
              mockedAppendFileSync(filepath, data, opts);
              process.nextTick(callback, null);
            } catch (err) {
              process.nextTick(callback, err);
            }
          };
          defineMockedProperty(fs, 'appendFile', mockedAppendFile, this.#originals.appendFile);

          const mockedPromisesAppendFile = async function appendFile(filepath, data, opts) {
            // Always append to virtual fs when mock is enabled.
            mockedAppendFileSync(filepath, data, opts);
          };
          defineMockedProperty(fsPromises, 'appendFile', mockedPromisesAppendFile, this.#originals.promisesAppendFile);
        },

        stat() {
          this.#originals.stat = getAndValidateDescriptor(fs, 'stat', 'fs');
          this.#originals.statSync = getAndValidateDescriptor(fs, 'statSync', 'fs');
          this.#originals.promisesStat = getAndValidateDescriptor(fsPromises, 'stat', 'fs/promises');

          const origStatSync = this.#originals.statSync.value;
          const origStat = this.#originals.stat.value;
          const origPromisesStat = this.#originals.promisesStat.value;

          const mockedStatSync = function statSync(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const file = self.#files.get(normalizedPath);

            if (file) {
              return file.stat;
            }

            if (self.#directories.has(normalizedPath)) {
              return createMockStats({ __proto__: null, isDirectory: true });
            }

            if (self.#isolate) {
              if (opts?.throwIfNoEntry === false) {
                return undefined;
              }
              throw createENOENT('stat', normalizedPath);
            }

            return origStatSync(filepath, opts);
          };
          defineMockedProperty(fs, 'statSync', mockedStatSync, this.#originals.statSync);

          const mockedStat = function stat(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                const result = mockedStatSync(filepath, opts);
                process.nextTick(callback, null, result);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origStat(filepath, opts, callback);
            }
          };
          defineMockedProperty(fs, 'stat', mockedStat, this.#originals.stat);

          const mockedPromisesStat = async function stat(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              return mockedStatSync(filepath, opts);
            }

            return origPromisesStat(filepath, opts);
          };
          defineMockedProperty(fsPromises, 'stat', mockedPromisesStat, this.#originals.promisesStat);
        },

        lstat() {
          this.#originals.lstat = getAndValidateDescriptor(fs, 'lstat', 'fs');
          this.#originals.lstatSync = getAndValidateDescriptor(fs, 'lstatSync', 'fs');
          this.#originals.promisesLstat = getAndValidateDescriptor(fsPromises, 'lstat', 'fs/promises');

          const origLstatSync = this.#originals.lstatSync.value;
          const origLstat = this.#originals.lstat.value;
          const origPromisesLstat = this.#originals.promisesLstat.value;

          // No symlink support - lstat behaves like stat for virtual files.
          const mockedLstatSync = function lstatSync(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);

            const file = self.#files.get(normalizedPath);
            if (file) {
              return file.stat;
            }

            if (self.#directories.has(normalizedPath)) {
              return createMockStats({ __proto__: null, isDirectory: true });
            }

            if (self.#isolate) {
              if (opts?.throwIfNoEntry === false) {
                return undefined;
              }
              throw createENOENT('lstat', normalizedPath);
            }

            return origLstatSync(filepath, opts);
          };
          defineMockedProperty(fs, 'lstatSync', mockedLstatSync, this.#originals.lstatSync);

          const mockedLstat = function lstat(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                const result = mockedLstatSync(filepath, opts);
                process.nextTick(callback, null, result);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origLstat(filepath, opts, callback);
            }
          };
          defineMockedProperty(fs, 'lstat', mockedLstat, this.#originals.lstat);

          const mockedPromisesLstat = async function lstat(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              return mockedLstatSync(filepath, opts);
            }

            return origPromisesLstat(filepath, opts);
          };
          defineMockedProperty(fsPromises, 'lstat', mockedPromisesLstat, this.#originals.promisesLstat);
        },

        access() {
          this.#originals.access = getAndValidateDescriptor(fs, 'access', 'fs');
          this.#originals.accessSync = getAndValidateDescriptor(fs, 'accessSync', 'fs');
          this.#originals.promisesAccess = getAndValidateDescriptor(fsPromises, 'access', 'fs/promises');

          const origAccessSync = this.#originals.accessSync.value;
          const origAccess = this.#originals.access.value;
          const origPromisesAccess = this.#originals.promisesAccess.value;

          const mockedAccessSync = function accessSync(filepath, mode) {
            const normalizedPath = self.#normalizePath(filepath);

            if (self.#virtualExists(normalizedPath)) {
              return undefined;
            }

            if (self.#isolate) {
              throw createENOENT('access', normalizedPath);
            }

            return origAccessSync(filepath, mode);
          };
          defineMockedProperty(fs, 'accessSync', mockedAccessSync, this.#originals.accessSync);

          const mockedAccess = function access(filepath, mode, callback) {
            if (typeof mode === 'function') {
              callback = mode;
              mode = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                mockedAccessSync(filepath, mode);
                process.nextTick(callback, null);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origAccess(filepath, mode, callback);
            }
          };
          defineMockedProperty(fs, 'access', mockedAccess, this.#originals.access);

          const mockedPromisesAccess = async function access(filepath, mode) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              mockedAccessSync(filepath, mode);
              return;
            }

            return origPromisesAccess(filepath, mode);
          };
          defineMockedProperty(fsPromises, 'access', mockedPromisesAccess, this.#originals.promisesAccess);
        },

        exists() {
          this.#originals.exists = getAndValidateDescriptor(fs, 'exists', 'fs');
          this.#originals.existsSync = getAndValidateDescriptor(fs, 'existsSync', 'fs');

          const origExists = this.#originals.exists.value;
          const origExistsSync = this.#originals.existsSync.value;

          const mockedExistsSync = function existsSync(filepath) {
            const normalizedPath = self.#normalizePath(filepath);

            if (self.#virtualExists(normalizedPath)) {
              return true;
            }

            if (self.#isolate) {
              return false;
            }

            return origExistsSync(filepath);
          };
          defineMockedProperty(fs, 'existsSync', mockedExistsSync, this.#originals.existsSync);

          const mockedExists = function exists(filepath, callback) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              process.nextTick(callback, isVirtual);
            } else {
              origExists(filepath, callback);
            }
          };
          defineMockedProperty(fs, 'exists', mockedExists, this.#originals.exists);
        },

        unlink() {
          this.#originals.unlink = getAndValidateDescriptor(fs, 'unlink', 'fs');
          this.#originals.unlinkSync = getAndValidateDescriptor(fs, 'unlinkSync', 'fs');
          this.#originals.promisesUnlink = getAndValidateDescriptor(fsPromises, 'unlink', 'fs/promises');

          const origUnlinkSync = this.#originals.unlinkSync.value;
          const origUnlink = this.#originals.unlink.value;
          const origPromisesUnlink = this.#originals.promisesUnlink.value;

          const mockedUnlinkSync = function unlinkSync(filepath) {
            const normalizedPath = self.#normalizePath(filepath);

            if (self.#files.has(normalizedPath)) {
              self.#files.delete(normalizedPath);
              return undefined;
            }

            if (self.#directories.has(normalizedPath)) {
              throw createEISDIR('unlink', normalizedPath);
            }

            if (self.#isolate) {
              throw createENOENT('unlink', normalizedPath);
            }

            return origUnlinkSync(filepath);
          };
          defineMockedProperty(fs, 'unlinkSync', mockedUnlinkSync, this.#originals.unlinkSync);

          const mockedUnlink = function unlink(filepath, callback) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                mockedUnlinkSync(filepath);
                process.nextTick(callback, null);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origUnlink(filepath, callback);
            }
          };
          defineMockedProperty(fs, 'unlink', mockedUnlink, this.#originals.unlink);

          const mockedPromisesUnlink = async function unlink(filepath) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              mockedUnlinkSync(filepath);
              return;
            }

            return origPromisesUnlink(filepath);
          };
          defineMockedProperty(fsPromises, 'unlink', mockedPromisesUnlink, this.#originals.promisesUnlink);
        },

        mkdir() {
          this.#originals.mkdir = getAndValidateDescriptor(fs, 'mkdir', 'fs');
          this.#originals.mkdirSync = getAndValidateDescriptor(fs, 'mkdirSync', 'fs');
          this.#originals.promisesMkdir = getAndValidateDescriptor(fsPromises, 'mkdir', 'fs/promises');

          const mockedMkdirSync = function mkdirSync(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const recursive = opts?.recursive ?? false;

            if (self.#virtualExists(normalizedPath)) {
              if (recursive) {
                return undefined;
              }
              throw createEEXIST('mkdir', normalizedPath);
            }

            // Always create directories in virtual fs when mock is enabled.
            // This keeps test environments isolated from the real filesystem.
            const parentDir = pathDirname(normalizedPath);
            const isParentRoot = parentDir === pathDirname(parentDir);

            // For non-recursive mkdir, require parent to exist (or be root).
            if (!recursive && !self.#directories.has(parentDir) && !isParentRoot) {
              throw createENOENT('mkdir', normalizedPath);
            }

            if (recursive) {
              // Collect directories that need to be created, from root to target.
              const dirsToCreate = [];
              let dir = normalizedPath;
              while (dir !== pathDirname(dir) && !self.#directories.has(dir)) {
                ArrayPrototypeUnshift(dirsToCreate, dir);
                dir = pathDirname(dir);
              }

              // Create all directories.
              for (let i = 0; i < dirsToCreate.length; i++) {
                self.#directories.add(dirsToCreate[i]);
              }

              // Return the first directory created, or undefined if none.
              return dirsToCreate.length > 0 ? dirsToCreate[0] : undefined;
            }

            self.#directories.add(normalizedPath);
            return undefined;
          };
          defineMockedProperty(fs, 'mkdirSync', mockedMkdirSync, this.#originals.mkdirSync);

          const mockedMkdir = function mkdir(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            // Always use virtual fs when mock is enabled
            try {
              const result = mockedMkdirSync(filepath, opts);
              process.nextTick(callback, null, result);
            } catch (err) {
              process.nextTick(callback, err);
            }
          };
          defineMockedProperty(fs, 'mkdir', mockedMkdir, this.#originals.mkdir);

          const mockedPromisesMkdir = async function mkdir(filepath, opts) {
            // Always use virtual fs when mock is enabled
            return mockedMkdirSync(filepath, opts);
          };
          defineMockedProperty(fsPromises, 'mkdir', mockedPromisesMkdir, this.#originals.promisesMkdir);
        },

        rmdir() {
          this.#originals.rmdir = getAndValidateDescriptor(fs, 'rmdir', 'fs');
          this.#originals.rmdirSync = getAndValidateDescriptor(fs, 'rmdirSync', 'fs');
          this.#originals.promisesRmdir = getAndValidateDescriptor(fsPromises, 'rmdir', 'fs/promises');

          const origRmdir = this.#originals.rmdir.value;
          const origRmdirSync = this.#originals.rmdirSync.value;
          const origPromisesRmdir = this.#originals.promisesRmdir.value;

          const mockedRmdirSync = function rmdirSync(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);

            if (self.#directories.has(normalizedPath)) {
              const prefix = normalizedPath + pathSep;
              MapPrototypeForEach(self.#files, (_, filePath) => {
                if (StringPrototypeStartsWith(filePath, prefix)) {
                  throw createENOTEMPTY('rmdir', normalizedPath);
                }
              });
              SetPrototypeForEach(self.#directories, (dirPath) => {
                if (dirPath !== normalizedPath && StringPrototypeStartsWith(dirPath, prefix)) {
                  throw createENOTEMPTY('rmdir', normalizedPath);
                }
              });
              self.#directories.delete(normalizedPath);
              return undefined;
            }

            if (self.#files.has(normalizedPath)) {
              throw createENOTDIR('rmdir', normalizedPath);
            }

            if (self.#isolate) {
              throw createENOENT('rmdir', normalizedPath);
            }

            return origRmdirSync(filepath, opts);
          };
          defineMockedProperty(fs, 'rmdirSync', mockedRmdirSync, this.#originals.rmdirSync);

          const mockedRmdir = function rmdir(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                mockedRmdirSync(filepath, opts);
                process.nextTick(callback, null);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origRmdir(filepath, opts, callback);
            }
          };
          defineMockedProperty(fs, 'rmdir', mockedRmdir, this.#originals.rmdir);

          const mockedPromisesRmdir = async function rmdir(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              mockedRmdirSync(filepath, opts);
              return;
            }

            return origPromisesRmdir(filepath, opts);
          };
          defineMockedProperty(fsPromises, 'rmdir', mockedPromisesRmdir, this.#originals.promisesRmdir);
        },

        readdir() {
          this.#originals.readdir = getAndValidateDescriptor(fs, 'readdir', 'fs');
          this.#originals.readdirSync = getAndValidateDescriptor(fs, 'readdirSync', 'fs');
          this.#originals.promisesReaddir = getAndValidateDescriptor(fsPromises, 'readdir', 'fs/promises');

          const origReaddir = this.#originals.readdir.value;
          const origReaddirSync = this.#originals.readdirSync.value;
          const origPromisesReaddir = this.#originals.promisesReaddir.value;

          const mockedReaddirSync = function readdirSync(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);

            const isRoot = normalizedPath === pathDirname(normalizedPath);
            const isVirtualDir = self.#directories.has(normalizedPath);

            if (isVirtualDir || isRoot) {
              // Check if recursive option is used - not supported in mock fs
              if (opts?.recursive) {
                throw new ERR_INVALID_ARG_VALUE(
                  'options.recursive',
                  true,
                  'is not supported by mock file system',
                );
              }

              const entries = new SafeSet();
              // For root path, don't double up the separator.
              const prefix = StringPrototypeEndsWith(normalizedPath, pathSep) ?
                normalizedPath :
                normalizedPath + pathSep;

              MapPrototypeForEach(self.#files, (_, filePath) => {
                if (StringPrototypeStartsWith(filePath, prefix)) {
                  const relativePath = StringPrototypeSlice(filePath, prefix.length);
                  const firstSegment = getFirstPathSegment(relativePath);
                  entries.add(firstSegment);
                }
              });

              SetPrototypeForEach(self.#directories, (dirPath) => {
                if (StringPrototypeStartsWith(dirPath, prefix)) {
                  const relativePath = StringPrototypeSlice(dirPath, prefix.length);
                  const firstSegment = getFirstPathSegment(relativePath);
                  entries.add(firstSegment);
                }
              });

              const result = ArrayFrom(entries);

              if (opts?.withFileTypes) {
                return ArrayPrototypeMap(result, (name) => {
                  const fullPath = pathJoin(normalizedPath, name);
                  const isDir = self.#directories.has(fullPath);
                  return {
                    __proto__: null,
                    name,
                    parentPath: normalizedPath,
                    path: normalizedPath,
                    isFile() { return !isDir; },
                    isDirectory() { return isDir; },
                    isBlockDevice() { return false; },
                    isCharacterDevice() { return false; },
                    isSymbolicLink() { return false; },
                    isFIFO() { return false; },
                    isSocket() { return false; },
                  };
                });
              }

              return result;
            }

            if (self.#files.has(normalizedPath)) {
              throw createENOTDIR('scandir', normalizedPath);
            }

            if (self.#isolate) {
              // Check if recursive option is used - not supported in mock fs
              if (opts?.recursive) {
                throw new ERR_INVALID_ARG_VALUE(
                  'options.recursive',
                  true,
                  'is not supported by mock file system',
                );
              }
              throw createENOENT('scandir', normalizedPath);
            }

            return origReaddirSync(filepath, opts);
          };
          defineMockedProperty(fs, 'readdirSync', mockedReaddirSync, this.#originals.readdirSync);

          const mockedReaddir = function readdir(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                const result = mockedReaddirSync(filepath, opts);
                process.nextTick(callback, null, result);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origReaddir(filepath, opts, callback);
            }
          };
          defineMockedProperty(fs, 'readdir', mockedReaddir, this.#originals.readdir);

          const mockedPromisesReaddir = async function readdir(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              return mockedReaddirSync(filepath, opts);
            }

            return origPromisesReaddir(filepath, opts);
          };
          defineMockedProperty(fsPromises, 'readdir', mockedPromisesReaddir, this.#originals.promisesReaddir);
        },
      },

      toReal: {
        '__proto__': null,
        readFile() {
          ObjectDefineProperty(fs, 'readFile', this.#originals.readFile);
          ObjectDefineProperty(fs, 'readFileSync', this.#originals.readFileSync);
          ObjectDefineProperty(fsPromises, 'readFile', this.#originals.promisesReadFile);
        },
        writeFile() {
          ObjectDefineProperty(fs, 'writeFile', this.#originals.writeFile);
          ObjectDefineProperty(fs, 'writeFileSync', this.#originals.writeFileSync);
          ObjectDefineProperty(fsPromises, 'writeFile', this.#originals.promisesWriteFile);
        },
        appendFile() {
          ObjectDefineProperty(fs, 'appendFile', this.#originals.appendFile);
          ObjectDefineProperty(fs, 'appendFileSync', this.#originals.appendFileSync);
          ObjectDefineProperty(fsPromises, 'appendFile', this.#originals.promisesAppendFile);
        },
        stat() {
          ObjectDefineProperty(fs, 'stat', this.#originals.stat);
          ObjectDefineProperty(fs, 'statSync', this.#originals.statSync);
          ObjectDefineProperty(fsPromises, 'stat', this.#originals.promisesStat);
        },
        lstat() {
          ObjectDefineProperty(fs, 'lstat', this.#originals.lstat);
          ObjectDefineProperty(fs, 'lstatSync', this.#originals.lstatSync);
          ObjectDefineProperty(fsPromises, 'lstat', this.#originals.promisesLstat);
        },
        access() {
          ObjectDefineProperty(fs, 'access', this.#originals.access);
          ObjectDefineProperty(fs, 'accessSync', this.#originals.accessSync);
          ObjectDefineProperty(fsPromises, 'access', this.#originals.promisesAccess);
        },
        exists() {
          ObjectDefineProperty(fs, 'exists', this.#originals.exists);
          ObjectDefineProperty(fs, 'existsSync', this.#originals.existsSync);
        },
        unlink() {
          ObjectDefineProperty(fs, 'unlink', this.#originals.unlink);
          ObjectDefineProperty(fs, 'unlinkSync', this.#originals.unlinkSync);
          ObjectDefineProperty(fsPromises, 'unlink', this.#originals.promisesUnlink);
        },
        mkdir() {
          ObjectDefineProperty(fs, 'mkdir', this.#originals.mkdir);
          ObjectDefineProperty(fs, 'mkdirSync', this.#originals.mkdirSync);
          ObjectDefineProperty(fsPromises, 'mkdir', this.#originals.promisesMkdir);
        },
        rmdir() {
          ObjectDefineProperty(fs, 'rmdir', this.#originals.rmdir);
          ObjectDefineProperty(fs, 'rmdirSync', this.#originals.rmdirSync);
          ObjectDefineProperty(fsPromises, 'rmdir', this.#originals.promisesRmdir);
        },
        readdir() {
          ObjectDefineProperty(fs, 'readdir', this.#originals.readdir);
          ObjectDefineProperty(fs, 'readdirSync', this.#originals.readdirSync);
          ObjectDefineProperty(fsPromises, 'readdir', this.#originals.promisesReaddir);
        },
      },
    };

    const target = activate ? options.toFake : options.toReal;
    for (let i = 0; i < this.#apisInContext.length; i++) {
      FunctionPrototypeCall(target[this.#apisInContext[i]], this);
    }
  }

  /**
   * @param {MockFileSystemOptions} [options]
   */
  enable(options = kEmptyObject) {
    emitExperimentalWarning('File system mocking');

    if (this.#isEnabled) {
      throw new ERR_INVALID_STATE('MockFileSystem is already enabled');
    }

    validateObject(options, 'options');
    const {
      files = kEmptyObject,
      isolate = false,
      apis = SUPPORTED_APIS,
    } = options;
    validateObject(files, 'options.files');
    validateBoolean(isolate, 'options.isolate');
    validateStringArray(apis, 'options.apis');

    // Validate that all specified APIs are supported.
    ArrayPrototypeForEach(apis, (api) => {
      if (!ArrayPrototypeIncludes(SUPPORTED_APIS, api)) {
        throw new ERR_INVALID_ARG_VALUE(
          'options.apis',
          api,
          `option ${api} is not supported`,
        );
      }
    });

    this.#apisInContext = apis;
    this.#isolate = isolate;
    this.#populateFiles(files);
    this.#toggleEnableApis(true);
    this.#isEnabled = true;
  }

  reset() {
    if (!this.#isEnabled) return;

    this.#toggleEnableApis(false);
    this.#apisInContext = [];
    this.#files.clear();
    this.#directories.clear();
    this.#isolate = false;
    this.#isEnabled = false;
  }

  [SymbolDispose]() {
    this.reset();
  }
}

module.exports = { MockFileSystem };
