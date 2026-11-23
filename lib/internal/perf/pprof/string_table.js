// Copyright 2022 Datadog, Inc.
// This product includes software developed at Datadog
// (https://www.datadoghq.com/).
//
// Modified for inclusion in Node.js core: ported from the encoding half of
// https://github.com/DataDog/pprof-format/blob/53548398f38b16eb096cc04a75927316a68a9b54/src/string-table.ts
//
// SPDX-License-Identifier: MIT

'use strict';

const {
  ArrayPrototypePush,
  SafeMap,
} = primordials;

const { Buffer } = require('buffer');
const { utf8Write } = require('internal/buffer');
const {
  encodeNumber,
  encodeTag,
  kEncodeToBuffer,
  kMeasure,
  kTypeLengthDelim,
  measureTag,
  varintLength,
} = require('internal/perf/pprof/protobuf');

/**
 * The strings of a profile, such as function names, file names and the names
 * of sample types. Each distinct string is stored once, and the other
 * messages of the profile refer to strings by their index in the table, so
 * that a file name shared by many functions is encoded once. The first
 * string is always the empty string, so that index 0 means no string. The
 * UTF-8 bytes of a string are written straight into the output when the
 * table is encoded, so adding a string allocates no buffer.
 */
class StringTable {
  strings = [];
  #byteLengths = [];
  #positions = new SafeMap();
  #encodedLength = 0;

  /**
   * Creates a string table that holds the empty string.
   */
  constructor() {
    // The first string of a pprof string table is always the empty string.
    this.dedup('');
  }

  /**
   * Returns the index of a string, adding it to the table if needed.
   * @param {string} value The string to look up.
   * @returns {number} The index of the string in the table.
   */
  dedup(value) {
    let position = this.#positions.get(value);
    if (position === undefined) {
      const byteLength = Buffer.byteLength(value, 'utf8');
      position = ArrayPrototypePush(this.strings, value) - 1;
      this.#positions.set(value, position);
      ArrayPrototypePush(this.#byteLengths, byteLength);
      this.#encodedLength += varintLength(byteLength) + byteLength;
    }
    return position;
  }

  /**
   * Measures the strings as the repeated string field `field`.
   * @param {number} field The field number of the string table, as declared
   *   in the `.proto` file.
   * @returns {number} The number of bytes that the field takes.
   */
  [kMeasure](field) {
    return this.#byteLengths.length * measureTag(field) + this.#encodedLength;
  }

  /**
   * Encodes the strings as the repeated string field `field`.
   * @param {Buffer} buffer The buffer to write the field to, which is at least
   *   as large as measured.
   * @param {number} offset The offset at which to write the field.
   * @param {number} field The field number of the string table, as declared
   *   in the `.proto` file.
   * @returns {number} The offset after the field.
   */
  [kEncodeToBuffer](buffer, offset, field) {
    for (let i = 0; i < this.#byteLengths.length; i++) {
      const byteLength = this.#byteLengths[i];
      offset = encodeTag(buffer, offset, field, kTypeLengthDelim);
      offset = encodeNumber(buffer, offset, byteLength);
      if (byteLength > 0) {
        utf8Write(buffer, this.strings[i], offset, byteLength);
      }
      offset += byteLength;
    }
    return offset;
  }
}

module.exports = {
  StringTable,
};
