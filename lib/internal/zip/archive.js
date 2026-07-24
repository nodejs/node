'use strict';

// Archive serialization: `createZipArchive()`/`createZipArchiveSync()` and
// the `zipFiles()` on-disk variant, the `generateZipArchive()` generator they
// build on (auto-switching to Zip64 as offsets/counts overflow), and the
// shared archive-option normalizer.

const {
  ArrayPrototypePush,
  NumberMAX_SAFE_INTEGER,
  StringPrototypeEndsWith,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
} = primordials;

const {
  codes: {
    ERR_ZIP_ARCHIVE_TOO_LARGE,
  },
} = require('internal/errors');
const {
  validateBoolean,
  validateInteger,
  validateObject,
  validateString,
} = require('internal/validators');
const { isUint8Array } = require('internal/util/types');
const { Buffer } = require('buffer');
const { Readable } = require('stream');
const fs = require('fs');
const {
  EMPTY_BUFFER,
  SENTINEL16,
  kFinalize,
} = require('internal/zip/constants');
const {
  buildArchiveTrailer,
} = require('internal/zip/header-builders');
const {
  fsStatAsync,
  fsLstatAsync,
  fsReadlinkAsync,
} = require('internal/zip/fs-util');
const { ZipEntry } = require('internal/zip/entry');

/**
 * `createZipArchive()`/`createZipArchiveSync()` (and the `ZipBuffer`
 * `toBuffer()`/`toBufferSync()` methods that forward to them) take a single
 * optional `options` argument that doubles as a plain archive comment: a
 * string is shorthand for `{ comment: options }`.
 * @param {string | { comment?: string, baseOffset?: number }} [options]
 * @returns {{ comment: string | undefined, baseOffset: number }}
 */
function normalizeArchiveOptions(options) {
  if (options === undefined) return { comment: undefined, baseOffset: 0 };
  if (typeof options === 'string') return { comment: options, baseOffset: 0 };
  validateObject(options, 'options');
  const { comment, baseOffset = 0 } = options;
  // A Buffer comment is an internal convenience (used when round-tripping an
  // existing archive) that preserves the original bytes without forcing them
  // through a decode/re-encode cycle that would corrupt non-UTF-8 comments.
  if (comment !== undefined && !isUint8Array(comment)) {
    validateString(comment, 'options.comment');
  }
  validateInteger(baseOffset, 'options.baseOffset', 0, NumberMAX_SAFE_INTEGER);
  return { comment, baseOffset };
}

/**
 * Serializes `entries` (a (async) iterable of `ZipEntry`) into a `Readable`
 * stream of archive byte chunks, automatically switching to Zip64 structures
 * once the entry count or any offset/size exceeds the classic 32-/16-bit
 * limits.
 *
 * `options.baseOffset` shifts every local/central header offset the archive
 * records by that many bytes, so the emitted bytes are self-describing even
 * when something else is written before them - for example, appending the
 * archive after `baseOffset` bytes already written to the same file, rather
 * than at its start.
 * @param {Iterable<ZipEntry> | AsyncIterable<ZipEntry>} entries
 * @param {string | { comment?: string, baseOffset?: number }} [options]
 * @returns {Readable}
 */
function createZipArchive(entries, options) {
  return Readable.from(generateZipArchive(entries, options), { objectMode: false });
}

/**
 * Creates an archive from files on disk. `files` is an iterable of
 * `[sourcePath, entryName]` pairs - an array, a `Map`, the result of
 * `Object.entries()`, a generator, and so on. Each entry captures the file's
 * Unix mode and modification time; a directory becomes a directory entry and a
 * regular file's contents are streamed in without being buffered in memory.
 *
 * With `options.followSymlinks` (default `true`) a symbolic link is resolved
 * and archived as the file it points to; with it `false` the link itself is
 * stored as a symlink entry whose content is the target path.
 * @param {Iterable<[string, string]>} files
 * @param {string | { followSymlinks?: boolean, comment?: string, baseOffset?: number }} [options]
 * @returns {import('stream').Readable}
 */
function zipFiles(files, options) {
  const followSymlinks = options?.followSymlinks ?? true;
  validateBoolean(followSymlinks, 'options.followSymlinks');
  return createZipArchive(fileEntries(files, followSymlinks), options);
}

// Turn each `[sourcePath, entryName]` pair into a `ZipEntry`, stat-ing the
// source to capture its mode/mtime and picking the symlink/directory/file
// entry shape; the file-backed variant streams contents rather than buffering.
async function* fileEntries(files, followSymlinks) {
  for await (const pair of files) {
    const sourcePath = pair[0];
    const name = pair[1];
    validateString(sourcePath, 'sourcePath');
    validateString(name, 'name');
    // Following links resolves through to the target (stat); otherwise the
    // link itself is inspected (lstat) and stored as a symlink entry.
    const stats = followSymlinks ? await fsStatAsync(sourcePath) : await fsLstatAsync(sourcePath);
    const options = { __proto__: null, mode: stats.mode & 0o7777, modified: stats.mtime };
    if (stats.isSymbolicLink()) {
      yield ZipEntry.createSymlink(name, await fsReadlinkAsync(sourcePath), options);
    } else if (stats.isDirectory()) {
      const dirName = StringPrototypeEndsWith(name, '/') ? name : `${name}/`;
      yield await ZipEntry.create(dirName, EMPTY_BUFFER, options);
    } else {
      // Open the file lazily and hand ownership to the consumer at the yield.
      // If this generator is instead returned at the yield - the consumer
      // stopped before taking the entry (its output stream was destroyed as
      // this entry was being produced) - the read stream was never handed
      // over, so close it here rather than leak its descriptor.
      const stream = fs.createReadStream(sourcePath);
      let handedOff = false;
      try {
        yield ZipEntry.createStream(name, stream, options);
        handedOff = true;
      } finally {
        if (!handedOff) stream.destroy();
      }
    }
  }
}

// Encode the archive comment (a string, or raw bytes when round-tripping a
// non-UTF-8 comment) and enforce the 16-bit length field (sec. 4.3.16).
function normalizeCommentBuffer(comment) {
  if (comment === undefined) return EMPTY_BUFFER;
  const buffer = isUint8Array(comment) ? comment : Buffer.from(comment, 'utf8');
  if (buffer.length > SENTINEL16) {
    throw new ERR_ZIP_ARCHIVE_TOO_LARGE(
      'the archive comment must not exceed 65535 bytes when encoded as UTF-8');
  }
  return buffer;
}

// Dispose the entries still queued behind an interrupted serialization. A
// ZipEntry handed to the archive is owned by it, so when the consumer destroys
// the output stream early - which runs `generateZipArchive`'s `finally` - the
// entries it never reached must be released too (a streaming entry may hold a
// file read stream). The entry currently being serialized is torn down by the
// `for await ... of entry` loop's own return propagation, so only the queue
// behind it is handled here.
//
// A materialized source (an array, `Map`, `Object.entries()`) has already
// created every entry, so each remaining one is pulled and disposed. A lazy
// async source is instead `return()`ed: forcing it to produce every remaining
// entry could open thousands of descriptors just to close them, so it is
// signalled to stop and release its own in-flight resource (see
// `fileEntries()`).
async function disposeQueuedEntries(iterator, isAsync) {
  if (isAsync) {
    if (typeof iterator.return === 'function') await iterator.return();
    return;
  }
  for (let next = iterator.next(); !next.done; next = iterator.next()) {
    const entry = next.value;
    if (typeof entry?.[SymbolAsyncDispose] === 'function') await entry[SymbolAsyncDispose]();
  }
}

// Core serializer backing `createZipArchive()`: emits each entry's local header
// + data, then the central directory, then the end-of-central-directory record
// (APPNOTE sec. 4.3.6 archive layout), promoting to Zip64 end records
// (sec. 4.3.14/4.3.15) once any count/offset/size overflows its classic field.
//
// The entries are owned by this generator: it drives the iterator by hand
// (rather than `for await`) so that if the consumer destroys the returned
// stream partway - `Readable.from()` then calls this generator's `return()` -
// the `finally` can dispose every entry that was never fully serialized. A
// synchronous source is stepped without `await` so a pulled-but-not-yet-used
// entry cannot be stranded by a return injected at the `await`.
async function* generateZipArchive(entries, options) {
  const { comment, baseOffset } = normalizeArchiveOptions(options);
  const commentBuffer = normalizeCommentBuffer(comment);
  const centralHeaders = [];
  let pos = baseOffset;
  const isAsync = entries[SymbolAsyncIterator] !== undefined;
  const iterator = isAsync ? entries[SymbolAsyncIterator]() : entries[SymbolIterator]();
  let completed = false;
  try {
    while (true) {
      const next = isAsync ? await iterator.next() : iterator.next();
      if (next.done) break;
      const entry = next.value;
      const start = pos;
      for await (const chunk of entry) {
        yield chunk;
        pos += chunk.length;
      }
      ArrayPrototypePush(centralHeaders, entry[kFinalize](start));
    }
    const centralDirectoryOffset = pos;
    for (let i = 0; i < centralHeaders.length; i++) {
      const chunk = centralHeaders[i];
      yield chunk;
      pos += chunk.length;
    }
    const centralDirectorySize = pos - centralDirectoryOffset;
    const count = centralHeaders.length;
    const trailer = buildArchiveTrailer(count, centralDirectorySize, centralDirectoryOffset, commentBuffer);
    for (let i = 0; i < trailer.length; i++) yield trailer[i];
    completed = true;
  } finally {
    if (!completed) await disposeQueuedEntries(iterator, isAsync);
  }
}

/**
 * The synchronous counterpart of `createZipArchive()`. `entries` must be a
 * plain (synchronous) `Iterable` of entries that don't require an
 * asynchronous serialization pass - a streaming entry created with
 * `ZipEntry.createStream()` throws when its turn to serialize comes up, the
 * same as calling `entry[Symbol.iterator]()` on one directly. Blocks the
 * event loop and further JavaScript execution until the whole archive
 * (including any deflate passes) has been produced; see
 * `zipEntry.contentSync()`.
 * @param {Iterable<ZipEntry>} entries
 * @param {string | { comment?: string, baseOffset?: number }} [options]
 * @yields {Buffer}
 */
function* createZipArchiveSync(entries, options) {
  const { comment, baseOffset } = normalizeArchiveOptions(options);
  const commentBuffer = normalizeCommentBuffer(comment);
  const centralHeaders = [];
  let pos = baseOffset;
  const iterator = entries[SymbolIterator]();
  let completed = false;
  // As in generateZipArchive(), the entries are owned here: a streaming entry
  // throws when serialized synchronously, so on that throw (or an early
  // return of this generator) dispose the entry that failed and every entry
  // still queued behind it, releasing any sources they hold.
  let current = null;
  try {
    for (let next = iterator.next(); !next.done; next = iterator.next()) {
      current = next.value;
      const start = pos;
      for (const chunk of current) {
        yield chunk;
        pos += chunk.length;
      }
      ArrayPrototypePush(centralHeaders, current[kFinalize](start));
      current = null;
    }
    const centralDirectoryOffset = pos;
    for (let i = 0; i < centralHeaders.length; i++) {
      const chunk = centralHeaders[i];
      yield chunk;
      pos += chunk.length;
    }
    const centralDirectorySize = pos - centralDirectoryOffset;
    const count = centralHeaders.length;
    const trailer = buildArchiveTrailer(count, centralDirectorySize, centralDirectoryOffset, commentBuffer);
    for (let i = 0; i < trailer.length; i++) yield trailer[i];
    completed = true;
  } finally {
    if (!completed) {
      if (typeof current?.[SymbolDispose] === 'function') current[SymbolDispose]();
      for (let next = iterator.next(); !next.done; next = iterator.next()) {
        const entry = next.value;
        if (typeof entry?.[SymbolDispose] === 'function') entry[SymbolDispose]();
      }
    }
  }
}

module.exports = {
  normalizeArchiveOptions,
  createZipArchive,
  createZipArchiveSync,
  zipFiles,
};
