'use strict';

// Public entry point for ZIP archive support in `node:zlib`. The
// implementation is split across `internal/zip/`:
//
//   constants        shared signatures, flags, symbols, and small values
//   binary           bounds-checked reads and buffer coercion
//   content-size     the module-global in-memory decompression ceiling
//   dos              MS-DOS date/time and CP437 legacy name/text decoding
//   extra-fields     TLV extra-field parsing and building
//   headers          reader-side header structures and archive-end location
//   header-builders  writer-side header/record builders
//   compression      deflate/inflate/zstd plumbing and member decoding
//   fs-util          fd read/write helpers
//   entry            ZipEntry
//   archive          createZipArchive()/zipFiles() serialization
//   buffer           ZipBuffer
//   file             ZipFile
//
// This barrel re-exports only the surface `lib/zlib.js` consumes.

const { ZipEntry } = require('internal/zip/entry');
const { ZipBuffer } = require('internal/zip/buffer');
const { ZipFile } = require('internal/zip/file');
const {
  createZipArchive,
  createZipArchiveSync,
  zipFiles,
} = require('internal/zip/archive');
const {
  getMaxZipContentSize,
  setMaxZipContentSize,
} = require('internal/zip/content-size');

module.exports = {
  ZipEntry,
  ZipFile,
  ZipBuffer,
  createZipArchive,
  createZipArchiveSync,
  zipFiles,
  getMaxZipContentSize,
  setMaxZipContentSize,
};
