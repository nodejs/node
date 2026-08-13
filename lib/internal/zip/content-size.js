'use strict';

// The module-global default ceiling on in-memory member decompression, and
// its public getter/setter. Kept in its own module so every read path sees
// the one mutable value.

const { validateInteger } = require('internal/validators');

// A default ceiling on the uncompressed size that the buffering read paths
// (`ZipEntry.prototype.content()`, and therefore `ZipBuffer`/`ZipFile`
// `get()`) will materialize in memory when the caller does not pass an
// explicit `maxSize`. An archive whose central directory declares a member
// larger than this is rejected before any large allocation happens. Callers
// that need larger members can either pass a per-call `maxSize` or raise the
// module default with `setMaxZipContentSize()`. The streaming read paths
// (`contentIterator()`, `ZipFile.prototype.stream()`) are bounded-memory by
// design and are not subject to this default.
const DEFAULT_MAX_ZIP_CONTENT_SIZE = 256 * 1024 * 1024; // 256 MiB
let maxZipContentSize = DEFAULT_MAX_ZIP_CONTENT_SIZE;

/**
 * @returns {number}
 */
function getMaxZipContentSize() {
  return maxZipContentSize;
}

/**
 * @param {number} size
 * @returns {void}
 */
function setMaxZipContentSize(size) {
  validateInteger(size, 'size', 0);
  maxZipContentSize = size;
}

module.exports = {
  DEFAULT_MAX_ZIP_CONTENT_SIZE,
  getMaxZipContentSize,
  setMaxZipContentSize,
};
