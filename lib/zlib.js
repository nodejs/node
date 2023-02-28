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

'use strict';

const {
  ArrayBuffer,
  ArrayPrototypeForEach,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  FunctionPrototypeBind,
  MapPrototypeSet,
  MathAbs,
  MathMaxApply,
  NumberIsFinite,
  NumberIsNaN,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  ObjectSetPrototypeOf,
  PromiseResolve,
  ReflectApply,
  StringPrototypeStartsWith,
  Symbol,
  TypedArrayPrototypeFill,
  Uint32Array,
} = primordials;

const { validateStringAfterArrayBufferView } = require('internal/fs/utils');
const {
  codes: {
    ERR_BROTLI_INVALID_PARAM,
    ERR_BUFFER_TOO_LARGE,
    ERR_INVALID_ARG_TYPE,
    ERR_OUT_OF_RANGE,
    ERR_ZIP_UNSUPPORTED_COMPRESSION,
    ERR_ZLIB_INITIALIZATION_FAILED,
  },
  genericNodeError,
  hideStackFrames,
} = require('internal/errors');
const { Transform, finished } = require('stream');
const {
  deprecate,
  kEmptyObject,
  promisify,
} = require('internal/util');
const {
  isArrayBufferView,
  isAnyArrayBuffer,
  isUint8Array,
} = require('internal/util/types');
const archiveBinding = internalBinding('archive');
const binding = internalBinding('zlib');
const assert = require('internal/assert');
const {
  Buffer,
  kMaxLength
} = require('buffer');
const { owner_symbol } = require('internal/async_hooks').symbols;
const {
  validateFunction,
  validateNumber,
} = require('internal/validators');

const kFlushFlag = Symbol('kFlushFlag');
const kError = Symbol('kError');

const allConstants = internalBinding('constants');
const constants = allConstants.zlib;

const {
  // Zlib flush levels
  Z_NO_FLUSH, Z_BLOCK, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH,
  // Zlib option values
  Z_MIN_CHUNK, Z_MIN_WINDOWBITS, Z_MAX_WINDOWBITS, Z_MIN_LEVEL, Z_MAX_LEVEL,
  Z_MIN_MEMLEVEL, Z_MAX_MEMLEVEL, Z_DEFAULT_CHUNK, Z_DEFAULT_COMPRESSION,
  Z_DEFAULT_STRATEGY, Z_DEFAULT_WINDOWBITS, Z_DEFAULT_MEMLEVEL, Z_FIXED,
  // Node's compression stream modes (node_zlib_mode)
  DEFLATE, DEFLATERAW, INFLATE, INFLATERAW, GZIP, GUNZIP, UNZIP,
  BROTLI_DECODE, BROTLI_ENCODE,
  // Brotli operations (~flush levels)
  BROTLI_OPERATION_PROCESS, BROTLI_OPERATION_FLUSH,
  BROTLI_OPERATION_FINISH, BROTLI_OPERATION_EMIT_METADATA,
  // Libzip integrated compression algorithms
  ZIP_CM_STORE, ZIP_CM_DEFLATE,
  // Libzip integration file metadata
  ZIP_OPSYS_UNIX,
} = constants;

const {
  S_IFMT,
  S_IFBLK,
  S_IFCHR,
  S_IFDIR,
  S_IFIFO,
  S_IFLNK,
  S_IFREG,
  S_IFSOCK,
} = allConstants.fs;

// Translation table for return codes.
const codes = {
  Z_OK: constants.Z_OK,
  Z_STREAM_END: constants.Z_STREAM_END,
  Z_NEED_DICT: constants.Z_NEED_DICT,
  Z_ERRNO: constants.Z_ERRNO,
  Z_STREAM_ERROR: constants.Z_STREAM_ERROR,
  Z_DATA_ERROR: constants.Z_DATA_ERROR,
  Z_MEM_ERROR: constants.Z_MEM_ERROR,
  Z_BUF_ERROR: constants.Z_BUF_ERROR,
  Z_VERSION_ERROR: constants.Z_VERSION_ERROR
};

for (const ckey of ObjectKeys(codes)) {
  codes[codes[ckey]] = ckey;
}

const identitySync = (buffer) => buffer;
const identity = (buffer, cb) => PromiseResolve(buffer);

const deflateRawSync = createConvenienceMethod(DeflateRaw, true);
const deflateRaw = createConvenienceMethod(DeflateRaw, false);

const inflateRawSync = createConvenienceMethod(InflateRaw, true);
const inflateRaw = createConvenienceMethod(InflateRaw, false);

const ZIP_INTEGRATED_COMPRESSION_ALGORITHMS = {
  [ZIP_CM_STORE]: {
    sync: {
      compress: identitySync,
      decompress: identitySync,
    },
    async: {
      compress: identity,
      decompress: identity,
    }
  },
  [ZIP_CM_DEFLATE]: {
    sync: {
      compress: deflateRawSync,
      decompress: inflateRawSync,
    },
    async: {
      compress: promisify(deflateRaw),
      decompress: promisify(inflateRaw),
    }
  },
};

function crc32Sync(buf) {
  let crc = ~0;

  for (let t = 0, T = buf.length; t < T; ++t)
    crc = (crc >>> 8) ^ CRC_32_TAB[(crc ^ buf[t]) & 0xff];

  return MathAbs(crc ^ -1);
}

const CRC_32_TAB = [
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
  0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
  0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
  0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
  0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
  0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
  0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
  0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
  0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
  0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
  0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
  0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
  0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
  0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
  0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
  0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
  0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
  0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
  0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
  0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
  0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
  0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
  0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
  0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
  0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
  0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
  0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
  0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
  0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
  0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
  0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
  0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
  0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
  0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
  0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
  0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
  0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
  0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
  0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
  0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
  0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
  0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
  0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
  0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
  0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
  0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
  0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
  0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
  0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
  0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
  0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
  0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
  0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
  0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
];

class ZipStats {
  constructor(size, compressionSize, compressionAlgorithm, mtime, crc, opsys, attributes) {
    this.mode = opsys === ZIP_OPSYS_UNIX ? attributes >>> 16 : 0;
    this.size = size;

    this.compressionSize = compressionSize;
    this.compressionAlgorithm = compressionAlgorithm;

    this.mtime = new primordials.Date(mtime);
    this.mtimeMs = this.mtime.getTime() * 1000;

    this.opsys = opsys;
    this.attributes = attributes;

    this.crc = crc;
  }

  isDirectory() {
    return (this.mode & S_IFMT) === S_IFDIR;
  }

  isFile() {
    return (this.mode & S_IFMT) === S_IFREG;
  }

  isBlockDevice() {
    return (this.mode & S_IFMT) === S_IFBLK;
  }

  isCharacterDevice() {
    return (this.mode & S_IFMT) === S_IFCHR;
  }

  isSymbolicLink() {
    return (this.mode & S_IFMT) === S_IFLNK;
  }

  isFIFO() {
    return (this.mode & S_IFMT) === S_IFIFO;
  }

  isSocket() {
    return (this.mode & S_IFMT) === S_IFSOCK;
  }
}

class ZipEnt {
  #type;

  constructor(name, entry, type) {
    this.name = name;
    this.entry = entry;
    this.#type = type;
  }

  isDirectory() {
    return this.#type === S_IFDIR;
  }

  isFile() {
    return this.#type === S_IFREG;
  }

  isBlockDevice() {
    return this.#type === S_IFBLK;
  }

  isCharacterDevice() {
    return this.#type === S_IFCHR;
  }

  isSymbolicLink() {
    return this.#type === S_IFLNK;
  }

  isFIFO() {
    return this.#type === S_IFIFO;
  }

  isSocket() {
    return this.#type === S_IFSOCK;
  }
}

function generateSimpleToc(entry, index) {
  return [entry[0], index];
}

function generateZipentToc(entry, index) {
  return [entry[0], new ZipEnt(entry[0], index, entry[1])];
}

class ZipArchive {
  #zip;

  constructor(data = null) {
    this.#zip = new archiveBinding.ZipArchive(data);
  }

  getEntries({ withFileTypes = false } = kEmptyObject) {
    const toc = this.#zip.getEntries(withFileTypes);

    const mapFn = withFileTypes ?
      generateZipentToc :
      generateSimpleToc;

    const res = new primordials.Map();
    for (let t = 0; t < toc.length; ++t) {
      const { 0: key, 1: val } = mapFn(toc[t], t);
      MapPrototypeSet(res, key, val);
    }

    return res;
  }

  addDirectory(p) {
    return this.#zip.addDirectory(p);
  }

  addSymlink(p, target) {
    const entry = this.addFile(p, target);

    this.restatEntry(entry, {
      opsys: ZIP_OPSYS_UNIX,
      attributes: (constants.S_IFLNK | 0o777) << 16,
    });

    return entry;
  }

  addFile(p, data, {
    algorithm = ZIP_CM_DEFLATE,
    compress = true,
    encoding = 'utf8',
    tolerateStore = true,
    crc, size
  } = kEmptyObject) {
    if (!isArrayBufferView(data)) {
      validateStringAfterArrayBufferView(data, 'data');
      data = Buffer.from(data, encoding);
    }

    if (compress) {
      if (!ObjectPrototypeHasOwnProperty(ZIP_INTEGRATED_COMPRESSION_ALGORITHMS, algorithm))
        throw new ERR_ZIP_UNSUPPORTED_COMPRESSION();

      const { compress: compressFn } = ZIP_INTEGRATED_COMPRESSION_ALGORITHMS[algorithm].sync;
      const compressed = compressFn(data);

      crc = crc32Sync(data);
      size = data.length;

      if (tolerateStore && compressed.length >= data.length) {
        algorithm = ZIP_CM_STORE;
      } else {
        data = compressed;
      }
    }

    return this.#zip.addEntry(p, algorithm, data, crc, size);
  }

  async addFilePromise(p, data, {
    algorithm = ZIP_CM_DEFLATE,
    compress = true,
    encoding = 'utf8',
    tolerateStore = true,
    crc, size
  } = kEmptyObject) {
    if (!isArrayBufferView(data)) {
      validateStringAfterArrayBufferView(data, 'data');
      data = Buffer.from(data, encoding);
    }

    if (compress) {
      if (!ObjectPrototypeHasOwnProperty(ZIP_INTEGRATED_COMPRESSION_ALGORITHMS, algorithm))
        throw new ERR_ZIP_UNSUPPORTED_COMPRESSION();

      const { compress: compressFn } = ZIP_INTEGRATED_COMPRESSION_ALGORITHMS[algorithm].async;
      const compressed = await compressFn(data);

      crc = crc32Sync(data);
      size = data.length;

      if (tolerateStore && compressed.length >= data.length) {
        algorithm = ZIP_CM_STORE;
      } else {
        data = compressed;
      }
    }

    return this.#zip.addEntry(p, data, algorithm, crc, size);
  }

  deleteEntry(entry) {
    this.#zip.deleteEntry(entry);
  }

  readEntry(entry, options = kEmptyObject) {
    if (typeof options === 'string')
      options = { encoding: options };

    const { decompress = true, encoding } = options;
    const { 0: algorithm, 1: compressed } = this.#zip.readEntry(entry);
    if (!decompress)
      return [algorithm, compressed];

    if (!ObjectPrototypeHasOwnProperty(ZIP_INTEGRATED_COMPRESSION_ALGORITHMS, algorithm))
      throw new ERR_ZIP_UNSUPPORTED_COMPRESSION();

    const { decompress: decompressFn } = ZIP_INTEGRATED_COMPRESSION_ALGORITHMS[algorithm].sync;
    const decompressed = decompressFn(compressed);

    return typeof encoding !== 'undefined' ? decompressed.toString(encoding) : decompressed;
  }

  async readEntryPromise(entry, options = kEmptyObject) {
    if (typeof options === 'string')
      options = { encoding: options };

    const { decompress = true, encoding } = options;
    const { 0: algorithm, 1: compressed } = this.#zip.readEntry(entry);
    if (!decompress)
      return [algorithm, compressed];

    if (!ObjectPrototypeHasOwnProperty(ZIP_INTEGRATED_COMPRESSION_ALGORITHMS, algorithm))
      throw new ERR_ZIP_UNSUPPORTED_COMPRESSION();

    const { decompress: decompressFn } = ZIP_INTEGRATED_COMPRESSION_ALGORITHMS[algorithm].async;
    const decompressed = await decompressFn(compressed);

    return typeof encoding !== 'undefined' ? decompressed.toString(encoding) : decompressed;
  }

  statEntry(entry) {
    const {
      0: size,
      1: compressionSize,
      2: compressionMethod,
      3: mtime,
      4: crc,
      5: opsys,
      6: attributes
    } = this.#zip.statEntry(entry);

    return new ZipStats(size, compressionSize, compressionMethod, mtime, crc, opsys, attributes);
  }

  restatEntry(entry, stats) {
    this.#zip.restatEntry(entry, stats);
  }

  digest(encoding) {
    const res = this.#zip.digest();

    return typeof encoding !== 'undefined' ?
      res.toString(encoding) :
      res;
  }
}

function zlibBuffer(engine, buffer, callback) {
  validateFunction(callback, 'callback');
  // Streams do not support non-Uint8Array ArrayBufferViews yet. Convert it to a
  // Buffer without copying.
  if (isArrayBufferView(buffer) && !isUint8Array(buffer)) {
    buffer = Buffer.from(buffer.buffer, buffer.byteOffset, buffer.byteLength);
  } else if (isAnyArrayBuffer(buffer)) {
    buffer = Buffer.from(buffer);
  }
  engine.buffers = null;
  engine.nread = 0;
  engine.cb = callback;
  engine.on('data', zlibBufferOnData);
  engine.on('error', zlibBufferOnError);
  engine.on('end', zlibBufferOnEnd);
  engine.end(buffer);
}

function zlibBufferOnData(chunk) {
  if (!this.buffers)
    this.buffers = [chunk];
  else
    ArrayPrototypePush(this.buffers, chunk);
  this.nread += chunk.length;
  if (this.nread > this._maxOutputLength) {
    this.close();
    this.removeAllListeners('end');
    this.cb(new ERR_BUFFER_TOO_LARGE(this._maxOutputLength));
  }
}

function zlibBufferOnError(err) {
  this.removeAllListeners('end');
  this.cb(err);
}

function zlibBufferOnEnd() {
  let buf;
  if (this.nread === 0) {
    buf = Buffer.alloc(0);
  } else {
    const bufs = this.buffers;
    buf = (bufs.length === 1 ? bufs[0] : Buffer.concat(bufs, this.nread));
  }
  this.close();
  if (this._info)
    this.cb(null, { buffer: buf, engine: this });
  else
    this.cb(null, buf);
}

function zlibBufferSync(engine, buffer) {
  if (typeof buffer === 'string') {
    buffer = Buffer.from(buffer);
  } else if (!isArrayBufferView(buffer)) {
    if (isAnyArrayBuffer(buffer)) {
      buffer = Buffer.from(buffer);
    } else {
      throw new ERR_INVALID_ARG_TYPE(
        'buffer',
        ['string', 'Buffer', 'TypedArray', 'DataView', 'ArrayBuffer'],
        buffer,
      );
    }
  }
  buffer = processChunkSync(engine, buffer, engine._finishFlushFlag);
  if (engine._info)
    return { buffer, engine };
  return buffer;
}

function zlibOnError(message, errno, code) {
  const self = this[owner_symbol];
  // There is no way to cleanly recover.
  // Continuing only obscures problems.

  const error = genericNodeError(message, { errno, code });
  error.errno = errno;
  error.code = code;
  self.destroy(error);
  self[kError] = error;
}

// 1. Returns false for undefined and NaN
// 2. Returns true for finite numbers
// 3. Throws ERR_INVALID_ARG_TYPE for non-numbers
// 4. Throws ERR_OUT_OF_RANGE for infinite numbers
const checkFiniteNumber = hideStackFrames((number, name) => {
  // Common case
  if (number === undefined) {
    return false;
  }

  if (NumberIsFinite(number)) {
    return true; // Is a valid number
  }

  if (NumberIsNaN(number)) {
    return false;
  }

  validateNumber(number, name);

  // Infinite numbers
  throw new ERR_OUT_OF_RANGE(name, 'a finite number', number);
});

// 1. Returns def for number when it's undefined or NaN
// 2. Returns number for finite numbers >= lower and <= upper
// 3. Throws ERR_INVALID_ARG_TYPE for non-numbers
// 4. Throws ERR_OUT_OF_RANGE for infinite numbers or numbers > upper or < lower
const checkRangesOrGetDefault = hideStackFrames(
  (number, name, lower, upper, def) => {
    if (!checkFiniteNumber(number, name)) {
      return def;
    }
    if (number < lower || number > upper) {
      throw new ERR_OUT_OF_RANGE(name,
                                 `>= ${lower} and <= ${upper}`, number);
    }
    return number;
  },
);

const FLUSH_BOUND = [
  [ Z_NO_FLUSH, Z_BLOCK ],
  [ BROTLI_OPERATION_PROCESS, BROTLI_OPERATION_EMIT_METADATA ],
];
const FLUSH_BOUND_IDX_NORMAL = 0;
const FLUSH_BOUND_IDX_BROTLI = 1;

// The base class for all Zlib-style streams.
function ZlibBase(opts, mode, handle, { flush, finishFlush, fullFlush }) {
  let chunkSize = Z_DEFAULT_CHUNK;
  let maxOutputLength = kMaxLength;
  // The ZlibBase class is not exported to user land, the mode should only be
  // passed in by us.
  assert(typeof mode === 'number');
  assert(mode >= DEFLATE && mode <= BROTLI_ENCODE);

  let flushBoundIdx;
  if (mode !== BROTLI_ENCODE && mode !== BROTLI_DECODE) {
    flushBoundIdx = FLUSH_BOUND_IDX_NORMAL;
  } else {
    flushBoundIdx = FLUSH_BOUND_IDX_BROTLI;
  }

  if (opts) {
    chunkSize = opts.chunkSize;
    if (!checkFiniteNumber(chunkSize, 'options.chunkSize')) {
      chunkSize = Z_DEFAULT_CHUNK;
    } else if (chunkSize < Z_MIN_CHUNK) {
      throw new ERR_OUT_OF_RANGE('options.chunkSize',
                                 `>= ${Z_MIN_CHUNK}`, chunkSize);
    }

    flush = checkRangesOrGetDefault(
      opts.flush, 'options.flush',
      FLUSH_BOUND[flushBoundIdx][0], FLUSH_BOUND[flushBoundIdx][1], flush);

    finishFlush = checkRangesOrGetDefault(
      opts.finishFlush, 'options.finishFlush',
      FLUSH_BOUND[flushBoundIdx][0], FLUSH_BOUND[flushBoundIdx][1],
      finishFlush);

    maxOutputLength = checkRangesOrGetDefault(
      opts.maxOutputLength, 'options.maxOutputLength',
      1, kMaxLength, kMaxLength);

    if (opts.encoding || opts.objectMode || opts.writableObjectMode) {
      opts = { ...opts };
      opts.encoding = null;
      opts.objectMode = false;
      opts.writableObjectMode = false;
    }
  }

  ReflectApply(Transform, this, [{ autoDestroy: true, ...opts }]);
  this[kError] = null;
  this.bytesWritten = 0;
  this._handle = handle;
  handle[owner_symbol] = this;
  // Used by processCallback() and zlibOnError()
  handle.onerror = zlibOnError;
  this._outBuffer = Buffer.allocUnsafe(chunkSize);
  this._outOffset = 0;

  this._chunkSize = chunkSize;
  this._defaultFlushFlag = flush;
  this._finishFlushFlag = finishFlush;
  this._defaultFullFlushFlag = fullFlush;
  this._info = opts && opts.info;
  this._maxOutputLength = maxOutputLength;
}
ObjectSetPrototypeOf(ZlibBase.prototype, Transform.prototype);
ObjectSetPrototypeOf(ZlibBase, Transform);

ObjectDefineProperty(ZlibBase.prototype, '_closed', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    return !this._handle;
  }
});

// `bytesRead` made sense as a name when looking from the zlib engine's
// perspective, but it is inconsistent with all other streams exposed by Node.js
// that have this concept, where it stands for the number of bytes read
// *from* the stream (that is, net.Socket/tls.Socket & file system streams).
ObjectDefineProperty(ZlibBase.prototype, 'bytesRead', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get: deprecate(function() {
    return this.bytesWritten;
  }, 'zlib.bytesRead is deprecated and will change its meaning in the ' +
     'future. Use zlib.bytesWritten instead.', 'DEP0108'),
  set: deprecate(function(value) {
    this.bytesWritten = value;
  }, 'Setting zlib.bytesRead is deprecated. ' +
     'This feature will be removed in the future.', 'DEP0108')
});

ZlibBase.prototype.reset = function() {
  if (!this._handle)
    assert(false, 'zlib binding closed');
  return this._handle.reset();
};

// This is the _flush function called by the transform class,
// internally, when the last chunk has been written.
ZlibBase.prototype._flush = function(callback) {
  this._transform(Buffer.alloc(0), '', callback);
};

// Force Transform compat behavior.
ZlibBase.prototype._final = function(callback) {
  callback();
};

// If a flush is scheduled while another flush is still pending, a way to figure
// out which one is the "stronger" flush is needed.
// This is currently only used to figure out which flush flag to use for the
// last chunk.
// Roughly, the following holds:
// Z_NO_FLUSH (< Z_TREES) < Z_BLOCK < Z_PARTIAL_FLUSH <
//     Z_SYNC_FLUSH < Z_FULL_FLUSH < Z_FINISH
const flushiness = [];
let i = 0;
const kFlushFlagList = [Z_NO_FLUSH, Z_BLOCK, Z_PARTIAL_FLUSH,
                        Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH];
for (const flushFlag of kFlushFlagList) {
  flushiness[flushFlag] = i++;
}

function maxFlush(a, b) {
  return flushiness[a] > flushiness[b] ? a : b;
}

// Set up a list of 'special' buffers that can be written using .write()
// from the .flush() code as a way of introducing flushing operations into the
// write sequence.
const kFlushBuffers = [];
{
  const dummyArrayBuffer = new ArrayBuffer();
  for (const flushFlag of kFlushFlagList) {
    kFlushBuffers[flushFlag] = Buffer.from(dummyArrayBuffer);
    kFlushBuffers[flushFlag][kFlushFlag] = flushFlag;
  }
}

ZlibBase.prototype.flush = function(kind, callback) {
  if (typeof kind === 'function' || (kind === undefined && !callback)) {
    callback = kind;
    kind = this._defaultFullFlushFlag;
  }

  if (this.writableFinished) {
    if (callback)
      process.nextTick(callback);
  } else if (this.writableEnded) {
    if (callback)
      this.once('end', callback);
  } else {
    this.write(kFlushBuffers[kind], '', callback);
  }
};

ZlibBase.prototype.close = function(callback) {
  if (callback) finished(this, callback);
  this.destroy();
};

ZlibBase.prototype._destroy = function(err, callback) {
  _close(this);
  callback(err);
};

ZlibBase.prototype._transform = function(chunk, encoding, cb) {
  let flushFlag = this._defaultFlushFlag;
  // We use a 'fake' zero-length chunk to carry information about flushes from
  // the public API to the actual stream implementation.
  if (typeof chunk[kFlushFlag] === 'number') {
    flushFlag = chunk[kFlushFlag];
  }

  // For the last chunk, also apply `_finishFlushFlag`.
  if (this.writableEnded && this.writableLength === chunk.byteLength) {
    flushFlag = maxFlush(flushFlag, this._finishFlushFlag);
  }
  processChunk(this, chunk, flushFlag, cb);
};

ZlibBase.prototype._processChunk = function(chunk, flushFlag, cb) {
  // _processChunk() is left for backwards compatibility
  if (typeof cb === 'function')
    processChunk(this, chunk, flushFlag, cb);
  else
    return processChunkSync(this, chunk, flushFlag);
};

function processChunkSync(self, chunk, flushFlag) {
  let availInBefore = chunk.byteLength;
  let availOutBefore = self._chunkSize - self._outOffset;
  let inOff = 0;
  let availOutAfter;
  let availInAfter;

  let buffers = null;
  let nread = 0;
  let inputRead = 0;
  const state = self._writeState;
  const handle = self._handle;
  let buffer = self._outBuffer;
  let offset = self._outOffset;
  const chunkSize = self._chunkSize;

  let error;
  self.on('error', function onError(er) {
    error = er;
  });

  while (true) {
    handle.writeSync(flushFlag,
                     chunk, // in
                     inOff, // in_off
                     availInBefore, // in_len
                     buffer, // out
                     offset, // out_off
                     availOutBefore); // out_len
    if (error)
      throw error;
    else if (self[kError])
      throw self[kError];

    availOutAfter = state[0];
    availInAfter = state[1];

    const inDelta = (availInBefore - availInAfter);
    inputRead += inDelta;

    const have = availOutBefore - availOutAfter;
    if (have > 0) {
      const out = buffer.slice(offset, offset + have);
      offset += have;
      if (!buffers)
        buffers = [out];
      else
        ArrayPrototypePush(buffers, out);
      nread += out.byteLength;

      if (nread > self._maxOutputLength) {
        _close(self);
        throw new ERR_BUFFER_TOO_LARGE(self._maxOutputLength);
      }

    } else {
      assert(have === 0, 'have should not go down');
    }

    // Exhausted the output buffer, or used all the input create a new one.
    if (availOutAfter === 0 || offset >= chunkSize) {
      availOutBefore = chunkSize;
      offset = 0;
      buffer = Buffer.allocUnsafe(chunkSize);
    }

    if (availOutAfter === 0) {
      // Not actually done. Need to reprocess.
      // Also, update the availInBefore to the availInAfter value,
      // so that if we have to hit it a third (fourth, etc.) time,
      // it'll have the correct byte counts.
      inOff += inDelta;
      availInBefore = availInAfter;
    } else {
      break;
    }
  }

  self.bytesWritten = inputRead;
  _close(self);

  if (nread === 0)
    return Buffer.alloc(0);

  return (buffers.length === 1 ? buffers[0] : Buffer.concat(buffers, nread));
}

function processChunk(self, chunk, flushFlag, cb) {
  const handle = self._handle;
  if (!handle) return process.nextTick(cb);

  handle.buffer = chunk;
  handle.cb = cb;
  handle.availOutBefore = self._chunkSize - self._outOffset;
  handle.availInBefore = chunk.byteLength;
  handle.inOff = 0;
  handle.flushFlag = flushFlag;

  handle.write(flushFlag,
               chunk, // in
               0, // in_off
               handle.availInBefore, // in_len
               self._outBuffer, // out
               self._outOffset, // out_off
               handle.availOutBefore); // out_len
}

function processCallback() {
  // This callback's context (`this`) is the `_handle` (ZCtx) object. It is
  // important to null out the values once they are no longer needed since
  // `_handle` can stay in memory long after the buffer is needed.
  const handle = this;
  const self = this[owner_symbol];
  const state = self._writeState;

  if (self.destroyed) {
    this.buffer = null;
    this.cb();
    return;
  }

  const availOutAfter = state[0];
  const availInAfter = state[1];

  const inDelta = handle.availInBefore - availInAfter;
  self.bytesWritten += inDelta;

  const have = handle.availOutBefore - availOutAfter;
  if (have > 0) {
    const out = self._outBuffer.slice(self._outOffset, self._outOffset + have);
    self._outOffset += have;
    self.push(out);
  } else {
    assert(have === 0, 'have should not go down');
  }

  if (self.destroyed) {
    this.cb();
    return;
  }

  // Exhausted the output buffer, or used all the input create a new one.
  if (availOutAfter === 0 || self._outOffset >= self._chunkSize) {
    handle.availOutBefore = self._chunkSize;
    self._outOffset = 0;
    self._outBuffer = Buffer.allocUnsafe(self._chunkSize);
  }

  if (availOutAfter === 0) {
    // Not actually done. Need to reprocess.
    // Also, update the availInBefore to the availInAfter value,
    // so that if we have to hit it a third (fourth, etc.) time,
    // it'll have the correct byte counts.
    handle.inOff += inDelta;
    handle.availInBefore = availInAfter;

    this.write(handle.flushFlag,
               this.buffer, // in
               handle.inOff, // in_off
               handle.availInBefore, // in_len
               self._outBuffer, // out
               self._outOffset, // out_off
               self._chunkSize); // out_len
    return;
  }

  if (availInAfter > 0) {
    // If we have more input that should be written, but we also have output
    // space available, that means that the compression library was not
    // interested in receiving more data, and in particular that the input
    // stream has ended early.
    // This applies to streams where we don't check data past the end of
    // what was consumed; that is, everything except Gunzip/Unzip.
    self.push(null);
  }

  // Finished with the chunk.
  this.buffer = null;
  this.cb();
}

function _close(engine) {
  // Caller may invoke .close after a zlib error (which will null _handle).
  if (!engine._handle)
    return;

  engine._handle.close();
  engine._handle = null;
}

const zlibDefaultOpts = {
  flush: Z_NO_FLUSH,
  finishFlush: Z_FINISH,
  fullFlush: Z_FULL_FLUSH
};
// Base class for all streams actually backed by zlib and using zlib-specific
// parameters.
function Zlib(opts, mode) {
  let windowBits = Z_DEFAULT_WINDOWBITS;
  let level = Z_DEFAULT_COMPRESSION;
  let memLevel = Z_DEFAULT_MEMLEVEL;
  let strategy = Z_DEFAULT_STRATEGY;
  let dictionary;

  if (opts) {
    // windowBits is special. On the compression side, 0 is an invalid value.
    // But on the decompression side, a value of 0 for windowBits tells zlib
    // to use the window size in the zlib header of the compressed stream.
    if ((opts.windowBits == null || opts.windowBits === 0) &&
        (mode === INFLATE ||
         mode === GUNZIP ||
         mode === UNZIP)) {
      windowBits = 0;
    } else {
      // `{ windowBits: 8 }` is valid for deflate but not gzip.
      const min = Z_MIN_WINDOWBITS + (mode === GZIP ? 1 : 0);
      windowBits = checkRangesOrGetDefault(
        opts.windowBits, 'options.windowBits',
        min, Z_MAX_WINDOWBITS, Z_DEFAULT_WINDOWBITS);
    }

    level = checkRangesOrGetDefault(
      opts.level, 'options.level',
      Z_MIN_LEVEL, Z_MAX_LEVEL, Z_DEFAULT_COMPRESSION);

    memLevel = checkRangesOrGetDefault(
      opts.memLevel, 'options.memLevel',
      Z_MIN_MEMLEVEL, Z_MAX_MEMLEVEL, Z_DEFAULT_MEMLEVEL);

    strategy = checkRangesOrGetDefault(
      opts.strategy, 'options.strategy',
      Z_DEFAULT_STRATEGY, Z_FIXED, Z_DEFAULT_STRATEGY);

    dictionary = opts.dictionary;
    if (dictionary !== undefined && !isArrayBufferView(dictionary)) {
      if (isAnyArrayBuffer(dictionary)) {
        dictionary = Buffer.from(dictionary);
      } else {
        throw new ERR_INVALID_ARG_TYPE(
          'options.dictionary',
          ['Buffer', 'TypedArray', 'DataView', 'ArrayBuffer'],
          dictionary,
        );
      }
    }
  }

  const handle = new binding.Zlib(mode);
  // Ideally, we could let ZlibBase() set up _writeState. I haven't been able
  // to come up with a good solution that doesn't break our internal API,
  // and with it all supported npm versions at the time of writing.
  this._writeState = new Uint32Array(2);
  handle.init(windowBits,
              level,
              memLevel,
              strategy,
              this._writeState,
              processCallback,
              dictionary);

  ReflectApply(ZlibBase, this, [opts, mode, handle, zlibDefaultOpts]);

  this._level = level;
  this._strategy = strategy;
}
ObjectSetPrototypeOf(Zlib.prototype, ZlibBase.prototype);
ObjectSetPrototypeOf(Zlib, ZlibBase);

// This callback is used by `.params()` to wait until a full flush happened
// before adjusting the parameters. In particular, the call to the native
// `params()` function should not happen while a write is currently in progress
// on the threadpool.
function paramsAfterFlushCallback(level, strategy, callback) {
  assert(this._handle, 'zlib binding closed');
  this._handle.params(level, strategy);
  if (!this.destroyed) {
    this._level = level;
    this._strategy = strategy;
    if (callback) callback();
  }
}

Zlib.prototype.params = function params(level, strategy, callback) {
  checkRangesOrGetDefault(level, 'level', Z_MIN_LEVEL, Z_MAX_LEVEL);
  checkRangesOrGetDefault(strategy, 'strategy', Z_DEFAULT_STRATEGY, Z_FIXED);

  if (this._level !== level || this._strategy !== strategy) {
    this.flush(Z_SYNC_FLUSH,
               FunctionPrototypeBind(paramsAfterFlushCallback, this,
                                     level, strategy, callback));
  } else {
    process.nextTick(callback);
  }
};

// generic zlib
// minimal 2-byte header
function Deflate(opts) {
  if (!(this instanceof Deflate))
    return new Deflate(opts);
  ReflectApply(Zlib, this, [opts, DEFLATE]);
}
ObjectSetPrototypeOf(Deflate.prototype, Zlib.prototype);
ObjectSetPrototypeOf(Deflate, Zlib);

function Inflate(opts) {
  if (!(this instanceof Inflate))
    return new Inflate(opts);
  ReflectApply(Zlib, this, [opts, INFLATE]);
}
ObjectSetPrototypeOf(Inflate.prototype, Zlib.prototype);
ObjectSetPrototypeOf(Inflate, Zlib);

function Gzip(opts) {
  if (!(this instanceof Gzip))
    return new Gzip(opts);
  ReflectApply(Zlib, this, [opts, GZIP]);
}
ObjectSetPrototypeOf(Gzip.prototype, Zlib.prototype);
ObjectSetPrototypeOf(Gzip, Zlib);

function Gunzip(opts) {
  if (!(this instanceof Gunzip))
    return new Gunzip(opts);
  ReflectApply(Zlib, this, [opts, GUNZIP]);
}
ObjectSetPrototypeOf(Gunzip.prototype, Zlib.prototype);
ObjectSetPrototypeOf(Gunzip, Zlib);

function DeflateRaw(opts) {
  if (opts && opts.windowBits === 8) opts.windowBits = 9;
  if (!(this instanceof DeflateRaw))
    return new DeflateRaw(opts);
  ReflectApply(Zlib, this, [opts, DEFLATERAW]);
}
ObjectSetPrototypeOf(DeflateRaw.prototype, Zlib.prototype);
ObjectSetPrototypeOf(DeflateRaw, Zlib);

function InflateRaw(opts) {
  if (!(this instanceof InflateRaw))
    return new InflateRaw(opts);
  ReflectApply(Zlib, this, [opts, INFLATERAW]);
}
ObjectSetPrototypeOf(InflateRaw.prototype, Zlib.prototype);
ObjectSetPrototypeOf(InflateRaw, Zlib);

function Unzip(opts) {
  if (!(this instanceof Unzip))
    return new Unzip(opts);
  ReflectApply(Zlib, this, [opts, UNZIP]);
}
ObjectSetPrototypeOf(Unzip.prototype, Zlib.prototype);
ObjectSetPrototypeOf(Unzip, Zlib);

function createConvenienceMethod(ctor, sync) {
  if (sync) {
    return function syncBufferWrapper(buffer, opts) {
      return zlibBufferSync(new ctor(opts), buffer);
    };
  }
  return function asyncBufferWrapper(buffer, opts, callback) {
    if (typeof opts === 'function') {
      callback = opts;
      opts = {};
    }
    return zlibBuffer(new ctor(opts), buffer, callback);
  };
}

const kMaxBrotliParam = MathMaxApply(ArrayPrototypeMap(
  ObjectKeys(constants),
  (key) => (StringPrototypeStartsWith(key, 'BROTLI_PARAM_') ?
    constants[key] :
    0),
));

const brotliInitParamsArray = new Uint32Array(kMaxBrotliParam + 1);

const brotliDefaultOpts = {
  flush: BROTLI_OPERATION_PROCESS,
  finishFlush: BROTLI_OPERATION_FINISH,
  fullFlush: BROTLI_OPERATION_FLUSH
};
function Brotli(opts, mode) {
  assert(mode === BROTLI_DECODE || mode === BROTLI_ENCODE);

  TypedArrayPrototypeFill(brotliInitParamsArray, -1);
  if (opts?.params) {
    ArrayPrototypeForEach(ObjectKeys(opts.params), (origKey) => {
      const key = +origKey;
      if (NumberIsNaN(key) || key < 0 || key > kMaxBrotliParam ||
          (brotliInitParamsArray[key] | 0) !== -1) {
        throw new ERR_BROTLI_INVALID_PARAM(origKey);
      }

      const value = opts.params[origKey];
      if (typeof value !== 'number' && typeof value !== 'boolean') {
        throw new ERR_INVALID_ARG_TYPE('options.params[key]',
                                       'number', opts.params[origKey]);
      }
      brotliInitParamsArray[key] = value;
    });
  }

  const handle = mode === BROTLI_DECODE ?
    new binding.BrotliDecoder(mode) : new binding.BrotliEncoder(mode);

  this._writeState = new Uint32Array(2);
  // TODO(addaleax): Sometimes we generate better error codes in C++ land,
  // e.g. ERR_BROTLI_PARAM_SET_FAILED -- it's hard to access them with
  // the current bindings setup, though.
  if (!handle.init(brotliInitParamsArray,
                   this._writeState,
                   processCallback)) {
    throw new ERR_ZLIB_INITIALIZATION_FAILED();
  }

  ReflectApply(ZlibBase, this, [opts, mode, handle, brotliDefaultOpts]);
}
ObjectSetPrototypeOf(Brotli.prototype, Zlib.prototype);
ObjectSetPrototypeOf(Brotli, Zlib);

function BrotliCompress(opts) {
  if (!(this instanceof BrotliCompress))
    return new BrotliCompress(opts);
  ReflectApply(Brotli, this, [opts, BROTLI_ENCODE]);
}
ObjectSetPrototypeOf(BrotliCompress.prototype, Brotli.prototype);
ObjectSetPrototypeOf(BrotliCompress, Brotli);

function BrotliDecompress(opts) {
  if (!(this instanceof BrotliDecompress))
    return new BrotliDecompress(opts);
  ReflectApply(Brotli, this, [opts, BROTLI_DECODE]);
}
ObjectSetPrototypeOf(BrotliDecompress.prototype, Brotli.prototype);
ObjectSetPrototypeOf(BrotliDecompress, Brotli);


function createProperty(ctor) {
  return {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: function(options) {
      return new ctor(options);
    }
  };
}

// Legacy alias on the C++ wrapper object. This is not public API, so we may
// want to runtime-deprecate it at some point. There's no hurry, though.
ObjectDefineProperty(binding.Zlib.prototype, 'jsref', {
  __proto__: null,
  get() { return this[owner_symbol]; },
  set(v) { return this[owner_symbol] = v; }
});

module.exports = {
  Deflate,
  Inflate,
  Gzip,
  Gunzip,
  DeflateRaw,
  InflateRaw,
  Unzip,
  BrotliCompress,
  BrotliDecompress,
  ZipArchive,
  crc32Sync,

  // Convenience methods.
  // compress/decompress a string or buffer in one step.
  deflate: createConvenienceMethod(Deflate, false),
  deflateSync: createConvenienceMethod(Deflate, true),
  gzip: createConvenienceMethod(Gzip, false),
  gzipSync: createConvenienceMethod(Gzip, true),
  deflateRaw: deflateRaw,
  deflateRawSync: deflateRawSync,
  unzip: createConvenienceMethod(Unzip, false),
  unzipSync: createConvenienceMethod(Unzip, true),
  inflate: createConvenienceMethod(Inflate, false),
  inflateSync: createConvenienceMethod(Inflate, true),
  gunzip: createConvenienceMethod(Gunzip, false),
  gunzipSync: createConvenienceMethod(Gunzip, true),
  inflateRaw: inflateRaw,
  inflateRawSync: inflateRawSync,
  brotliCompress: createConvenienceMethod(BrotliCompress, false),
  brotliCompressSync: createConvenienceMethod(BrotliCompress, true),
  brotliDecompress: createConvenienceMethod(BrotliDecompress, false),
  brotliDecompressSync: createConvenienceMethod(BrotliDecompress, true),
};

ObjectDefineProperties(module.exports, {
  createDeflate: createProperty(Deflate),
  createInflate: createProperty(Inflate),
  createDeflateRaw: createProperty(DeflateRaw),
  createInflateRaw: createProperty(InflateRaw),
  createGzip: createProperty(Gzip),
  createGunzip: createProperty(Gunzip),
  createUnzip: createProperty(Unzip),
  createBrotliCompress: createProperty(BrotliCompress),
  createBrotliDecompress: createProperty(BrotliDecompress),
  createZipArchive: createProperty(ZipArchive),
  constants: {
    __proto__: null,
    configurable: false,
    enumerable: true,
    value: constants
  },
  codes: {
    __proto__: null,
    enumerable: true,
    writable: false,
    value: ObjectFreeze(codes)
  }
});

// These should be considered deprecated
// expose all the zlib constants
for (const bkey of ObjectKeys(constants)) {
  if (StringPrototypeStartsWith(bkey, 'BROTLI')) continue;
  ObjectDefineProperty(module.exports, bkey, {
    __proto__: null,
    enumerable: false, value: constants[bkey], writable: false
  });
}
