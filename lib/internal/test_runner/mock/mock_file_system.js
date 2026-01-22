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
  MathCeil,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototype,
  ObjectPrototypeHasOwnProperty,
  ReflectConstruct,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  SafeSet,
  String,
  StringPrototypeEndsWith,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  SymbolDispose,
  hardenRegExp,
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

const BufferPrototypeToString = Buffer.prototype.toString;
const BufferFrom = Buffer.from;
const BufferConcat = Buffer.concat;
const BufferAlloc = Buffer.alloc;
const { UV_ENOENT, UV_ENOTDIR, UV_ENOTEMPTY, UV_EEXIST, UV_EISDIR } =
  internalBinding('uv');

const fs = require('fs');
const fsPromises = require('fs/promises');
const {
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

// Regex for splitting paths on forward or back slashes.
const kPathSeparatorRegex = hardenRegExp(/[\\/]/);

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
  const nowDate = ReflectConstruct(Date, [nowMs]);

  // Create object that inherits from Stats.prototype so instanceof checks work.
  // We use Object.create to avoid calling the deprecated Stats constructor.
  const stats = { __proto__: Stats.prototype };

  // Set all the standard stats properties.
  stats.dev = 0;
  stats.ino = 0;
  stats.mode = isDirectory ? S_IFDIR | mode : S_IFREG | mode;
  stats.nlink = 1;
  stats.uid = 0;
  stats.gid = 0;
  stats.rdev = 0;
  stats.size = size;
  stats.blksize = DEFAULT_BLKSIZE;
  stats.blocks = MathCeil(size / BLOCK_SIZE);
  stats.atimeMs = nowMs;
  stats.mtimeMs = nowMs;
  stats.ctimeMs = nowMs;
  stats.birthtimeMs = nowMs;
  stats.atime = nowDate;
  stats.mtime = nowDate;
  stats.ctime = nowDate;
  stats.birthtime = nowDate;

  return stats;
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
 * @param {string} base
 * @param {string} name
 * @returns {string}
 */
function safePathJoin(base, name) {
  if (StringPrototypeEndsWith(base, pathSep)) {
    return base + name;
  }
  return base + pathSep + name;
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
        FunctionPrototypeCall(BufferPrototypeToString, bufferPath, 'utf8'),
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
      const segments = RegExpPrototypeSymbolSplit(kPathSeparatorRegex, filepath);
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
        'readFile': () => {
          this.#originals.readFile = ObjectGetOwnPropertyDescriptor(fs, 'readFile');
          this.#originals.readFileSync = ObjectGetOwnPropertyDescriptor(fs, 'readFileSync');
          this.#originals.promisesReadFile = ObjectGetOwnPropertyDescriptor(fsPromises, 'readFile');

          const origReadFileSync = this.#originals.readFileSync.value;
          const origReadFile = this.#originals.readFile.value;
          const origPromisesReadFile = this.#originals.promisesReadFile.value;

          fs.readFileSync = function readFileSync(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const file = self.#files.get(normalizedPath);

            if (file) {
              const encoding = typeof opts === 'string' ? opts : opts?.encoding;
              if (encoding) {
                return FunctionPrototypeCall(BufferPrototypeToString, file.content, encoding);
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

          fs.readFile = function readFile(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#files.has(normalizedPath) || self.#directories.has(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                const result = fs.readFileSync(filepath, opts);
                process.nextTick(callback, null, result);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origReadFile(filepath, opts, callback);
            }
          };

          fsPromises.readFile = async function readFile(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#files.has(normalizedPath) || self.#directories.has(normalizedPath);

            if (isVirtual || self.#isolate) {
              return fs.readFileSync(filepath, opts);
            }

            return origPromisesReadFile(filepath, opts);
          };
        },

        'writeFile': () => {
          this.#originals.writeFile = ObjectGetOwnPropertyDescriptor(fs, 'writeFile');
          this.#originals.writeFileSync = ObjectGetOwnPropertyDescriptor(fs, 'writeFileSync');
          this.#originals.promisesWriteFile = ObjectGetOwnPropertyDescriptor(fsPromises, 'writeFile');

          fs.writeFileSync = function writeFileSync(filepath, data, opts) {
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

          fs.writeFile = function writeFile(filepath, data, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            // Always write to virtual fs when mock is enabled.
            try {
              fs.writeFileSync(filepath, data, opts);
              process.nextTick(callback, null);
            } catch (err) {
              process.nextTick(callback, err);
            }
          };

          fsPromises.writeFile = async function writeFile(filepath, data, opts) {
            // Always write to virtual fs when mock is enabled.
            fs.writeFileSync(filepath, data, opts);
          };
        },

        'appendFile': () => {
          this.#originals.appendFile = ObjectGetOwnPropertyDescriptor(fs, 'appendFile');
          this.#originals.appendFileSync = ObjectGetOwnPropertyDescriptor(fs, 'appendFileSync');
          this.#originals.promisesAppendFile = ObjectGetOwnPropertyDescriptor(fsPromises, 'appendFile');

          fs.appendFileSync = function appendFileSync(filepath, data, opts) {
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

          fs.appendFile = function appendFile(filepath, data, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            // Always append to virtual fs when mock is enabled.
            try {
              fs.appendFileSync(filepath, data, opts);
              process.nextTick(callback, null);
            } catch (err) {
              process.nextTick(callback, err);
            }
          };

          fsPromises.appendFile = async function appendFile(filepath, data, opts) {
            // Always append to virtual fs when mock is enabled.
            fs.appendFileSync(filepath, data, opts);
          };
        },

        'stat': () => {
          this.#originals.stat = ObjectGetOwnPropertyDescriptor(fs, 'stat');
          this.#originals.statSync = ObjectGetOwnPropertyDescriptor(fs, 'statSync');
          this.#originals.promisesStat = ObjectGetOwnPropertyDescriptor(fsPromises, 'stat');

          const origStatSync = this.#originals.statSync.value;
          const origStat = this.#originals.stat.value;
          const origPromisesStat = this.#originals.promisesStat.value;

          fs.statSync = function statSync(filepath, opts) {
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

          fs.stat = function stat(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                const result = fs.statSync(filepath, opts);
                process.nextTick(callback, null, result);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origStat(filepath, opts, callback);
            }
          };

          fsPromises.stat = async function stat(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              return fs.statSync(filepath, opts);
            }

            return origPromisesStat(filepath, opts);
          };
        },

        'lstat': () => {
          this.#originals.lstat = ObjectGetOwnPropertyDescriptor(fs, 'lstat');
          this.#originals.lstatSync = ObjectGetOwnPropertyDescriptor(fs, 'lstatSync');
          this.#originals.promisesLstat = ObjectGetOwnPropertyDescriptor(fsPromises, 'lstat');

          const origLstatSync = this.#originals.lstatSync.value;
          const origLstat = this.#originals.lstat.value;
          const origPromisesLstat = this.#originals.promisesLstat.value;

          fs.lstatSync = function lstatSync(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);

            // No symlink support - lstat behaves like stat for virtual files.
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

          fs.lstat = function lstat(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                const result = fs.lstatSync(filepath, opts);
                process.nextTick(callback, null, result);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origLstat(filepath, opts, callback);
            }
          };

          fsPromises.lstat = async function lstat(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              return fs.lstatSync(filepath, opts);
            }

            return origPromisesLstat(filepath, opts);
          };
        },

        'access': () => {
          this.#originals.access = ObjectGetOwnPropertyDescriptor(fs, 'access');
          this.#originals.accessSync = ObjectGetOwnPropertyDescriptor(fs, 'accessSync');
          this.#originals.promisesAccess = ObjectGetOwnPropertyDescriptor(fsPromises, 'access');

          const origAccessSync = this.#originals.accessSync.value;
          const origAccess = this.#originals.access.value;
          const origPromisesAccess = this.#originals.promisesAccess.value;

          fs.accessSync = function accessSync(filepath, mode) {
            const normalizedPath = self.#normalizePath(filepath);

            if (self.#virtualExists(normalizedPath)) {
              return undefined;
            }

            if (self.#isolate) {
              throw createENOENT('access', normalizedPath);
            }

            return origAccessSync(filepath, mode);
          };

          fs.access = function access(filepath, mode, callback) {
            if (typeof mode === 'function') {
              callback = mode;
              mode = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                fs.accessSync(filepath, mode);
                process.nextTick(callback, null);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origAccess(filepath, mode, callback);
            }
          };

          fsPromises.access = async function access(filepath, mode) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              fs.accessSync(filepath, mode);
              return;
            }

            return origPromisesAccess(filepath, mode);
          };
        },

        'exists': () => {
          this.#originals.exists = ObjectGetOwnPropertyDescriptor(fs, 'exists');
          this.#originals.existsSync = ObjectGetOwnPropertyDescriptor(fs, 'existsSync');

          const origExists = this.#originals.exists.value;
          const origExistsSync = this.#originals.existsSync.value;

          fs.existsSync = function existsSync(filepath) {
            const normalizedPath = self.#normalizePath(filepath);

            if (self.#virtualExists(normalizedPath)) {
              return true;
            }

            if (self.#isolate) {
              return false;
            }

            return origExistsSync(filepath);
          };

          fs.exists = function exists(filepath, callback) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              process.nextTick(callback, isVirtual);
            } else {
              origExists(filepath, callback);
            }
          };
        },

        'unlink': () => {
          this.#originals.unlink = ObjectGetOwnPropertyDescriptor(fs, 'unlink');
          this.#originals.unlinkSync = ObjectGetOwnPropertyDescriptor(fs, 'unlinkSync');
          this.#originals.promisesUnlink = ObjectGetOwnPropertyDescriptor(fsPromises, 'unlink');

          const origUnlinkSync = this.#originals.unlinkSync.value;
          const origUnlink = this.#originals.unlink.value;
          const origPromisesUnlink = this.#originals.promisesUnlink.value;

          fs.unlinkSync = function unlinkSync(filepath) {
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

          fs.unlink = function unlink(filepath, callback) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                fs.unlinkSync(filepath);
                process.nextTick(callback, null);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origUnlink(filepath, callback);
            }
          };

          fsPromises.unlink = async function unlink(filepath) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              fs.unlinkSync(filepath);
              return;
            }

            return origPromisesUnlink(filepath);
          };
        },

        'mkdir': () => {
          this.#originals.mkdir = ObjectGetOwnPropertyDescriptor(fs, 'mkdir');
          this.#originals.mkdirSync = ObjectGetOwnPropertyDescriptor(fs, 'mkdirSync');
          this.#originals.promisesMkdir = ObjectGetOwnPropertyDescriptor(fsPromises, 'mkdir');

          fs.mkdirSync = function mkdirSync(filepath, opts) {
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

          fs.mkdir = function mkdir(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            // Always use virtual fs when mock is enabled
            try {
              const result = fs.mkdirSync(filepath, opts);
              process.nextTick(callback, null, result);
            } catch (err) {
              process.nextTick(callback, err);
            }
          };

          fsPromises.mkdir = async function mkdir(filepath, opts) {
            // Always use virtual fs when mock is enabled
            return fs.mkdirSync(filepath, opts);
          };
        },

        'rmdir': () => {
          this.#originals.rmdir = ObjectGetOwnPropertyDescriptor(fs, 'rmdir');
          this.#originals.rmdirSync = ObjectGetOwnPropertyDescriptor(fs, 'rmdirSync');
          this.#originals.promisesRmdir = ObjectGetOwnPropertyDescriptor(fsPromises, 'rmdir');

          const origRmdir = this.#originals.rmdir.value;
          const origRmdirSync = this.#originals.rmdirSync.value;
          const origPromisesRmdir = this.#originals.promisesRmdir.value;

          fs.rmdirSync = function rmdirSync(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);

            if (self.#directories.has(normalizedPath)) {
              const prefix = normalizedPath + pathSep;
              const fileKeys = ArrayFrom(self.#files.keys());
              for (let i = 0; i < fileKeys.length; i++) {
                if (StringPrototypeStartsWith(fileKeys[i], prefix)) {
                  throw createENOTEMPTY('rmdir', normalizedPath);
                }
              }
              const dirKeys = ArrayFrom(self.#directories);
              for (let i = 0; i < dirKeys.length; i++) {
                if (dirKeys[i] !== normalizedPath && StringPrototypeStartsWith(dirKeys[i], prefix)) {
                  throw createENOTEMPTY('rmdir', normalizedPath);
                }
              }
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

          fs.rmdir = function rmdir(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                fs.rmdirSync(filepath, opts);
                process.nextTick(callback, null);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origRmdir(filepath, opts, callback);
            }
          };

          fsPromises.rmdir = async function rmdir(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              fs.rmdirSync(filepath, opts);
              return;
            }

            return origPromisesRmdir(filepath, opts);
          };
        },

        'readdir': () => {
          this.#originals.readdir = ObjectGetOwnPropertyDescriptor(fs, 'readdir');
          this.#originals.readdirSync = ObjectGetOwnPropertyDescriptor(fs, 'readdirSync');
          this.#originals.promisesReaddir = ObjectGetOwnPropertyDescriptor(fsPromises, 'readdir');

          const origReaddir = this.#originals.readdir.value;
          const origReaddirSync = this.#originals.readdirSync.value;
          const origPromisesReaddir = this.#originals.promisesReaddir.value;

          fs.readdirSync = function readdirSync(filepath, opts) {
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

              const fileKeys = ArrayFrom(self.#files.keys());
              for (let i = 0; i < fileKeys.length; i++) {
                const filePath = fileKeys[i];
                if (StringPrototypeStartsWith(filePath, prefix)) {
                  const relativePath = StringPrototypeSlice(filePath, prefix.length);
                  const firstSegment = getFirstPathSegment(relativePath);
                  entries.add(firstSegment);
                }
              }

              const dirKeys = ArrayFrom(self.#directories);
              for (let i = 0; i < dirKeys.length; i++) {
                const dirPath = dirKeys[i];
                if (StringPrototypeStartsWith(dirPath, prefix)) {
                  const relativePath = StringPrototypeSlice(dirPath, prefix.length);
                  const firstSegment = getFirstPathSegment(relativePath);
                  entries.add(firstSegment);
                }
              }

              const result = ArrayFrom(entries);

              if (opts?.withFileTypes) {
                return ArrayPrototypeMap(result, (name) => {
                  const fullPath = safePathJoin(normalizedPath, name);
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

          fs.readdir = function readdir(filepath, opts, callback) {
            if (typeof opts === 'function') {
              callback = opts;
              opts = undefined;
            }

            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              try {
                const result = fs.readdirSync(filepath, opts);
                process.nextTick(callback, null, result);
              } catch (err) {
                process.nextTick(callback, err);
              }
            } else {
              origReaddir(filepath, opts, callback);
            }
          };

          fsPromises.readdir = async function readdir(filepath, opts) {
            const normalizedPath = self.#normalizePath(filepath);
            const isVirtual = self.#virtualExists(normalizedPath);

            if (isVirtual || self.#isolate) {
              return fs.readdirSync(filepath, opts);
            }

            return origPromisesReaddir(filepath, opts);
          };
        },
      },

      toReal: {
        '__proto__': null,
        'readFile': () => {
          ObjectDefineProperty(fs, 'readFile', this.#originals.readFile);
          ObjectDefineProperty(fs, 'readFileSync', this.#originals.readFileSync);
          ObjectDefineProperty(fsPromises, 'readFile', this.#originals.promisesReadFile);
        },
        'writeFile': () => {
          ObjectDefineProperty(fs, 'writeFile', this.#originals.writeFile);
          ObjectDefineProperty(fs, 'writeFileSync', this.#originals.writeFileSync);
          ObjectDefineProperty(fsPromises, 'writeFile', this.#originals.promisesWriteFile);
        },
        'appendFile': () => {
          ObjectDefineProperty(fs, 'appendFile', this.#originals.appendFile);
          ObjectDefineProperty(fs, 'appendFileSync', this.#originals.appendFileSync);
          ObjectDefineProperty(fsPromises, 'appendFile', this.#originals.promisesAppendFile);
        },
        'stat': () => {
          ObjectDefineProperty(fs, 'stat', this.#originals.stat);
          ObjectDefineProperty(fs, 'statSync', this.#originals.statSync);
          ObjectDefineProperty(fsPromises, 'stat', this.#originals.promisesStat);
        },
        'lstat': () => {
          ObjectDefineProperty(fs, 'lstat', this.#originals.lstat);
          ObjectDefineProperty(fs, 'lstatSync', this.#originals.lstatSync);
          ObjectDefineProperty(fsPromises, 'lstat', this.#originals.promisesLstat);
        },
        'access': () => {
          ObjectDefineProperty(fs, 'access', this.#originals.access);
          ObjectDefineProperty(fs, 'accessSync', this.#originals.accessSync);
          ObjectDefineProperty(fsPromises, 'access', this.#originals.promisesAccess);
        },
        'exists': () => {
          ObjectDefineProperty(fs, 'exists', this.#originals.exists);
          ObjectDefineProperty(fs, 'existsSync', this.#originals.existsSync);
        },
        'unlink': () => {
          ObjectDefineProperty(fs, 'unlink', this.#originals.unlink);
          ObjectDefineProperty(fs, 'unlinkSync', this.#originals.unlinkSync);
          ObjectDefineProperty(fsPromises, 'unlink', this.#originals.promisesUnlink);
        },
        'mkdir': () => {
          ObjectDefineProperty(fs, 'mkdir', this.#originals.mkdir);
          ObjectDefineProperty(fs, 'mkdirSync', this.#originals.mkdirSync);
          ObjectDefineProperty(fsPromises, 'mkdir', this.#originals.promisesMkdir);
        },
        'rmdir': () => {
          ObjectDefineProperty(fs, 'rmdir', this.#originals.rmdir);
          ObjectDefineProperty(fs, 'rmdirSync', this.#originals.rmdirSync);
          ObjectDefineProperty(fsPromises, 'rmdir', this.#originals.promisesRmdir);
        },
        'readdir': () => {
          ObjectDefineProperty(fs, 'readdir', this.#originals.readdir);
          ObjectDefineProperty(fs, 'readdirSync', this.#originals.readdirSync);
          ObjectDefineProperty(fsPromises, 'readdir', this.#originals.promisesReaddir);
        },
      },
    };

    const target = activate ? options.toFake : options.toReal;
    ArrayPrototypeForEach(this.#apisInContext, (api) => target[api]());
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
