'use strict';

// Compression/decompression plumbing over the lazily-required `zlib` facade:
// one-shot (async and sync) and streaming deflate/inflate/zstd helpers, plus
// the member decoders that add method dispatch, size bounding, and CRC-32
// verification on top.

const {
  JSONStringify,
  MathMin,
  Promise,
} = primordials;

const {
  codes: {
    ERR_ZIP_ENTRY_CORRUPT,
    ERR_ZIP_ENTRY_TOO_LARGE,
    ERR_ZIP_UNSUPPORTED_FEATURE,
  },
} = require('internal/errors');
const { kMaxLength } = require('buffer');
const { compose } = require('stream');
const { crc32: crc32Native } = internalBinding('zlib');
const {
  FLAG_ENCRYPTED,
  METHOD_STORE,
  METHOD_DEFLATE,
  METHOD_ZSTD,
} = require('internal/zip/constants');

// `internal/zip` is required from `lib/zlib.js`, so it must not require the
// public `zlib` facade at load time (its module.exports is not yet
// populated). Compression is only needed once an entry is actually read or
// written, well after `zlib.js` has finished loading, so a lazy reference is
// enough to break the cycle.
let zlib;
function lazyZlib() {
  zlib ??= require('zlib');
  return zlib;
}

// -- compression plumbing ------------------------------------------------------

// The one-shot (async/sync) and streaming helpers below are thin adapters that
// promisify or stream-wrap the lazy `zlib` facade; individually trivial, they
// exist only so the rest of the module never touches `zlib` directly.

function deflateRawAsync(buffer) {
  return new Promise((resolve, reject) => {
    lazyZlib().deflateRaw(buffer, (err, result) => {
      if (err) reject(err);
      else resolve(result);
    });
  });
}

function inflateRawAsync(buffer, options) {
  return new Promise((resolve, reject) => {
    lazyZlib().inflateRaw(buffer, options, (err, result) => {
      if (err) reject(err);
      else resolve(result);
    });
  });
}

// Drive `source` through a zlib transform stream, returning an async-iterable
// stream of its output. `compose()` (pipeline-backed) wires error propagation
// both ways and tears the whole chain down when the consumer errors or stops
// early, so an abandoned iteration cannot leak the pipeline.
function pumpThroughTransform(source, transform) {
  return compose(source, transform);
}

function deflateRawStream(source) {
  return pumpThroughTransform(source, lazyZlib().createDeflateRaw());
}

function inflateRawStream(source) {
  return pumpThroughTransform(source, lazyZlib().createInflateRaw());
}

function zstdCompressAsync(buffer) {
  return new Promise((resolve, reject) => {
    lazyZlib().zstdCompress(buffer, (err, result) => {
      if (err) reject(err);
      else resolve(result);
    });
  });
}

function zstdDecompressAsync(buffer, options) {
  return new Promise((resolve, reject) => {
    lazyZlib().zstdDecompress(buffer, options, (err, result) => {
      if (err) reject(err);
      else resolve(result);
    });
  });
}

function zstdCompressStream(source) {
  return pumpThroughTransform(source, lazyZlib().createZstdCompress());
}

function zstdDecompressStream(source) {
  return pumpThroughTransform(source, lazyZlib().createZstdDecompress());
}


function deflateRawSync(buffer) {
  return lazyZlib().deflateRawSync(buffer);
}

function zstdCompressSync(buffer) {
  return lazyZlib().zstdCompressSync(buffer);
}

/**
 * @typedef {{
 *   name: string,
 *   flags: number,
 *   method: number,
 *   crc32: number,
 *   uncompressedSize: number,
 * }} ZipMemberInfo
 */

// Shared entry guards for the member decoders below: encryption and
// unsupported compression methods are rejected up front, and when the caller
// bounds the output, a declared size beyond that bound fails before anything
// is decompressed or allocated.
function assertDecodable(info, options) {
  if (info.flags & FLAG_ENCRYPTED) {
    throw new ERR_ZIP_UNSUPPORTED_FEATURE(
      `entry ${JSONStringify(info.name)} is encrypted`);
  }
  if (info.method !== METHOD_STORE && info.method !== METHOD_DEFLATE && info.method !== METHOD_ZSTD) {
    throw new ERR_ZIP_UNSUPPORTED_FEATURE(
      `entry ${JSONStringify(info.name)} uses compression method ${info.method}`);
  }
  if (options?.maxSize !== undefined && info.uncompressedSize > options.maxSize) {
    throw new ERR_ZIP_ENTRY_TOO_LARGE(
      `entry ${JSONStringify(info.name)} declares ${info.uncompressedSize} bytes, ` +
      `exceeding the ${options.maxSize} byte limit`);
  }
}

// Bound one-shot decompression by the declared size, not just the caller's
// limit: a member that decompresses to more than it declares is corrupt by
// definition, so there is no reason to materialize more than `declared + 1`
// bytes (the +1 makes an overrun detectable) no matter how generous `maxSize`
// is. This keeps a tiny archive from forcing a `maxSize`-sized allocation.
function outputCap(info, options) {
  return MathMin(
    info.uncompressedSize + 1, options?.maxSize ?? kMaxLength, kMaxLength);
}

// Map a one-shot decompression failure to a corrupt-entry error.
// `assertDecodable()` has already ensured `declared <= maxSize`, so hitting
// the output cap always means the stream produced more than the member
// declared.
function rethrowDecodeFailure(err, info, method) {
  if (err?.code === 'ERR_BUFFER_TOO_LARGE') {
    throw new ERR_ZIP_ENTRY_CORRUPT(
      `entry ${JSONStringify(info.name)} ` +
      `${method === METHOD_DEFLATE ? 'inflates' : 'decompresses'} beyond its ` +
      `declared size of ${info.uncompressedSize} bytes`);
  }
  throw new ERR_ZIP_ENTRY_CORRUPT(
    `entry ${JSONStringify(info.name)} failed to ` +
    `${method === METHOD_DEFLATE ? 'inflate' : 'decompress'}: ${err.message}`);
}

// Enforce the declared size and (unless opted out) the CRC-32 on a fully
// decoded member.
function checkDecoded(data, info, verify) {
  if (data.length !== info.uncompressedSize) {
    throw new ERR_ZIP_ENTRY_CORRUPT(
      `entry ${JSONStringify(info.name)} produced ${data.length} bytes, expected ` +
      `${info.uncompressedSize}`);
  }
  if (verify && crc32Native(data, 0) !== info.crc32) {
    throw new ERR_ZIP_ENTRY_CORRUPT(
      `entry ${JSONStringify(info.name)} failed CRC-32 verification`);
  }
}

/**
 * Decodes one member's compressed byte stream: rejects encrypted entries and
 * unsupported compression methods, inflates method 8 or decompresses method
 * 93 (Zstandard), enforces the declared uncompressed size and verifies
 * CRC-32 (on by default).
 * @param {AsyncIterable<Buffer>} source
 * @param {ZipMemberInfo} info
 * @param {{ verify?: boolean, maxSize?: number }} [options]
 * @yields {Buffer}
 */
async function* decodeMemberStream(source, info, options) {
  assertDecodable(info, options);
  const verify = options?.verify !== false;
  let produced = 0;
  let state = 0;
  const decoded = info.method === METHOD_DEFLATE ? inflateRawStream(source) :
    info.method === METHOD_ZSTD ? zstdDecompressStream(source) : source;
  for await (const chunk of decoded) {
    produced += chunk.length;
    if (produced > info.uncompressedSize) {
      throw new ERR_ZIP_ENTRY_CORRUPT(
        `entry ${JSONStringify(info.name)} inflates beyond its declared size of ` +
        `${info.uncompressedSize} bytes`);
    }
    if (verify) state = crc32Native(chunk, state);
    yield chunk;
  }
  if (produced !== info.uncompressedSize) {
    throw new ERR_ZIP_ENTRY_CORRUPT(
      `entry ${JSONStringify(info.name)} is truncated: got ${produced} of ` +
      `${info.uncompressedSize} bytes`);
  }
  if (verify && state !== info.crc32) {
    throw new ERR_ZIP_ENTRY_CORRUPT(
      `entry ${JSONStringify(info.name)} failed CRC-32 verification`);
  }
}

/**
 * Decodes one member held completely in `compressed`, in one shot: the same
 * guards, declared-size bounding, and CRC-32 verification as
 * `decodeMemberStream()`, returning the whole decoded member. For the store
 * method the input buffer itself is returned - callers that must hand out
 * caller-owned memory copy it (see `zipEntry.content()`).
 * @param {Buffer} compressed
 * @param {ZipMemberInfo} info
 * @param {{ verify?: boolean, maxSize?: number }} [options]
 * @returns {Promise<Buffer>}
 */
async function decodeMemberAsync(compressed, info, options) {
  assertDecodable(info, options);
  const cap = outputCap(info, options);
  let data;
  if (info.method === METHOD_DEFLATE) {
    try {
      data = await inflateRawAsync(compressed, { maxOutputLength: cap });
    } catch (err) {
      rethrowDecodeFailure(err, info, METHOD_DEFLATE);
    }
  } else if (info.method === METHOD_ZSTD) {
    try {
      data = await zstdDecompressAsync(compressed, { maxOutputLength: cap });
    } catch (err) {
      rethrowDecodeFailure(err, info, METHOD_ZSTD);
    }
  } else {
    data = compressed;
  }
  checkDecoded(data, info, options?.verify !== false);
  return data;
}

/**
 * The synchronous counterpart of `decodeMemberAsync()`. There is no public
 * synchronous incremental inflate API, so - unlike the streaming path -
 * `compressed` must already be the member's complete compressed byte
 * stream, and the whole result is produced (and verified) in one call
 * rather than yielded incrementally.
 * @param {Buffer} compressed
 * @param {ZipMemberInfo} info
 * @param {{ verify?: boolean, maxSize?: number }} [options]
 * @returns {Buffer}
 */
function decodeMemberSync(compressed, info, options) {
  assertDecodable(info, options);
  const cap = outputCap(info, options);
  let data;
  if (info.method === METHOD_DEFLATE) {
    try {
      data = lazyZlib().inflateRawSync(compressed, { maxOutputLength: cap });
    } catch (err) {
      rethrowDecodeFailure(err, info, METHOD_DEFLATE);
    }
  } else if (info.method === METHOD_ZSTD) {
    try {
      data = lazyZlib().zstdDecompressSync(compressed, { maxOutputLength: cap });
    } catch (err) {
      rethrowDecodeFailure(err, info, METHOD_ZSTD);
    }
  } else {
    data = compressed;
  }
  checkDecoded(data, info, options?.verify !== false);
  return data;
}

module.exports = {
  deflateRawAsync,
  deflateRawSync,
  zstdCompressAsync,
  zstdCompressSync,
  deflateRawStream,
  zstdCompressStream,
  decodeMemberStream,
  decodeMemberAsync,
  decodeMemberSync,
};
