// This file is a modified version of:
// https://cs.chromium.org/chromium/src/v8/tools/SourceMap.js?rcl=dd10454c1d
// from the V8 codebase. Logic specific to WebInspector is removed and linting
// is made to match the Node.js style guide.

// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This is a copy from blink dev tools, see:
// http://src.chromium.org/viewvc/blink/trunk/Source/devtools/front_end/SourceMap.js
// revision: 153407

/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  Int32Array,
  MathMax,
  ObjectFreeze,
  ObjectPrototypeHasOwnProperty,
  StringPrototypeCharAt,
  StringPrototypeCharCodeAt,
  Symbol,
  TypedArrayPrototypeSet,
  TypedArrayPrototypeSlice,
  TypedArrayPrototypeSubarray,
} = primordials;

const { validateObject } = require('internal/validators');

let base64Map;

const VLQ_BASE_SHIFT = 5;
const VLQ_BASE_MASK = (1 << 5) - 1;
const VLQ_CONTINUATION_MASK = 1 << 5;

const kFirstEntry = Symbol('kFirstEntry');

// Each entry is stored as kEntrySize consecutive Int32 fields: generated line,
// generated column, source index, original line, original column and name
// index. Large maps have millions of entries, and one array per entry costs
// several times more memory than the numbers it holds.
const kEntrySize = 6;
// Source index of an entry that has only a generated column.
const kNoSource = -1;
// Source index of an entry whose source index is outside `sources`.
const kUnknownSource = -2;
const kNoName = -1;
const kComma = 0x2C;
const kSemicolon = 0x3B;

class StringCharIterator {
  /**
   * @param {string} string
   */
  constructor(string) {
    this._string = string;
    this._position = 0;
  }

  /**
   * @returns {string}
   */
  next() {
    return StringPrototypeCharAt(this._string, this._position++);
  }

  /**
   * @returns {string}
   */
  peek() {
    return StringPrototypeCharAt(this._string, this._position);
  }

  /**
   * @returns {boolean}
   */
  hasNext() {
    return this._position < this._string.length;
  }
}

/**
 * @class
 * Implements Source Map V3 model.
 * See https://github.com/google/closure-compiler/wiki/Source-Maps
 * for format description.
 */
class SourceMap {
  #payload;
  #entries;
  #entryCount = 0;
  #sourceURLs = [];
  #names = [];
  #sources = {};
  #sourceContentByURL = {};
  #lineLengths = undefined;

  /**
   * @param {SourceMapV3} payload
   */
  constructor(payload, { lineLengths } = { __proto__: null }) {
    if (!base64Map) {
      const base64Digits =
             'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
      base64Map = {};
      for (let i = 0; i < base64Digits.length; ++i)
        base64Map[base64Digits[i]] = i;
    }
    this.#payload = cloneSourceMapV3(payload);
    this.#parseMappingPayload();
    if (ArrayIsArray(lineLengths) && lineLengths.length) {
      this.#lineLengths = ObjectFreeze(ArrayPrototypeSlice(lineLengths));
    }
  }

  /**
   * @returns {object} raw source map v3 payload.
   */
  get payload() {
    return this.#payload;
  }

  get [kFirstEntry]() {
    return this.#entryCount ? this.#entryAt(0) : {};
  }

  /**
   * @returns {number[] | undefined} line lengths of generated source code
   */
  get lineLengths() {
    return this.#lineLengths;
  }

  #parseMappingPayload = () => {
    const { sections } = this.#payload;
    let segments = 0;
    if (sections) {
      for (let i = 0; i < sections.length; ++i) {
        segments += countSegments(sections[i]?.map?.mappings);
      }
    } else {
      segments = countSegments(this.#payload.mappings);
    }
    this.#entries = new Int32Array(segments * kEntrySize);
    if (sections) {
      this.#parseSections(sections);
    } else {
      this.#parseMap(this.#payload, 0, 0);
    }
    this.#sortEntries();
  };

  #sortEntries() {
    const entries = this.#entries;
    const count = this.#entryCount;
    const compare = (a, b) => {
      const lineDelta = entries[a * kEntrySize] - entries[b * kEntrySize];
      return lineDelta ||
        entries[a * kEntrySize + 1] - entries[b * kEntrySize + 1];
    };
    let sorted = true;
    for (let i = 1; i < count; ++i) {
      if (compare(i - 1, i) > 0) {
        sorted = false;
        break;
      }
    }
    if (sorted) {
      if (entries.length !== count * kEntrySize) {
        this.#entries =
          TypedArrayPrototypeSlice(entries, 0, count * kEntrySize);
      }
      return;
    }
    // Sorting indices keeps equal positions in insertion order, like the
    // stable sort of entries this replaces.
    const order = [];
    for (let i = 0; i < count; ++i) {
      ArrayPrototypePush(order, i);
    }
    ArrayPrototypeSort(order, compare);
    const sortedEntries = new Int32Array(count * kEntrySize);
    for (let i = 0; i < count; ++i) {
      const offset = order[i] * kEntrySize;
      TypedArrayPrototypeSet(
        sortedEntries,
        TypedArrayPrototypeSubarray(entries, offset, offset + kEntrySize),
        i * kEntrySize,
      );
    }
    this.#entries = sortedEntries;
  }

  #pushEntry(lineNumber, columnNumber, sourceIndex, sourceLineNumber,
             sourceColumnNumber, nameIndex) {
    const offset = this.#entryCount * kEntrySize;
    if (offset === this.#entries.length) {
      // Only reached when the parser finds more segments than
      // countSegments(), which happens for malformed mappings.
      const grown =
        new Int32Array(MathMax(this.#entries.length * 2, kEntrySize * 16));
      TypedArrayPrototypeSet(grown, this.#entries);
      this.#entries = grown;
    }
    const entries = this.#entries;
    entries[offset] = lineNumber;
    entries[offset + 1] = columnNumber;
    entries[offset + 2] = sourceIndex;
    entries[offset + 3] = sourceLineNumber;
    entries[offset + 4] = sourceColumnNumber;
    entries[offset + 5] = nameIndex;
    this.#entryCount++;
  }

  #entryAt(index) {
    const entries = this.#entries;
    const offset = index * kEntrySize;
    const sourceIndex = entries[offset + 2];
    if (sourceIndex === kNoSource) {
      return {
        generatedLine: entries[offset],
        generatedColumn: entries[offset + 1],
        originalSource: undefined,
        originalLine: undefined,
        originalColumn: undefined,
        name: undefined,
      };
    }
    const nameIndex = entries[offset + 5];
    return {
      generatedLine: entries[offset],
      generatedColumn: entries[offset + 1],
      originalSource: sourceIndex === kUnknownSource ?
        undefined : this.#sourceURLs[sourceIndex],
      originalLine: entries[offset + 3],
      originalColumn: entries[offset + 4],
      name: nameIndex === kNoName ? undefined : this.#names[nameIndex],
    };
  }

  /**
   * @param {Array.<SourceMapV3.Section>} sections
   */
  #parseSections = (sections) => {
    for (let i = 0; i < sections.length; ++i) {
      const section = sections[i];
      this.#parseMap(section.map, section.offset.line, section.offset.column);
    }
  };

  /**
   * @param {number} lineOffset 0-indexed line offset in compiled resource
   * @param {number} columnOffset 0-indexed column offset in compiled resource
   * @returns {object} representing start of range if found, or empty object
   */
  findEntry(lineOffset, columnOffset) {
    const entries = this.#entries;
    let first = 0;
    let count = this.#entryCount;
    if (!count) {
      return {};
    }
    while (count > 1) {
      const step = count >> 1;
      const middle = first + step;
      const line = entries[middle * kEntrySize];
      if (lineOffset < line ||
          (lineOffset === line &&
           columnOffset < entries[middle * kEntrySize + 1])) {
        count = step;
      } else {
        first = middle;
        count -= step;
      }
    }
    if (!first && (lineOffset < entries[0] ||
        (lineOffset === entries[0] && columnOffset < entries[1]))) {
      return {};
    }
    return this.#entryAt(first);
  }

  /**
   * @param {number} lineNumber 1-indexed line number in compiled resource call site
   * @param {number} columnNumber 1-indexed column number in compiled resource call site
   * @returns {object} representing origin call site if found, or empty object
   */
  findOrigin(lineNumber, columnNumber) {
    const range = this.findEntry(lineNumber - 1, columnNumber - 1);
    if (
      range.originalSource === undefined ||
      range.originalLine === undefined ||
      range.originalColumn === undefined ||
      range.generatedLine === undefined ||
      range.generatedColumn === undefined
    ) {
      return {};
    }
    const lineOffset = lineNumber - range.generatedLine;
    const columnOffset = columnNumber - range.generatedColumn;
    return {
      name: range.name,
      fileName: range.originalSource,
      lineNumber: range.originalLine + lineOffset,
      columnNumber: range.originalColumn + columnOffset,
    };
  }

  /**
   * @override
   */
  #parseMap(map, lineNumber, columnNumber) {
    let sourceIndex = 0;
    let sourceLineNumber = 0;
    let sourceColumnNumber = 0;
    let nameIndex = 0;

    const sources = [];
    const originalToCanonicalURLMap = {};
    const sourceBase = this.#sourceURLs.length;
    for (let i = 0; i < map.sources.length; ++i) {
      const url = map.sources[i];
      originalToCanonicalURLMap[url] = url;
      ArrayPrototypePush(sources, url);
      ArrayPrototypePush(this.#sourceURLs, url);
      this.#sources[url] = true;

      if (map.sourcesContent?.[i])
        this.#sourceContentByURL[url] = map.sourcesContent[i];
    }
    const names = map.names ?? [];
    const nameBase = this.#names.length;
    for (let i = 0; i < names.length; ++i) {
      ArrayPrototypePush(this.#names, names[i]);
    }

    const stringCharIterator = new StringCharIterator(map.mappings);
    while (true) {
      if (stringCharIterator.peek() === ',')
        stringCharIterator.next();
      else {
        while (stringCharIterator.peek() === ';') {
          lineNumber += 1;
          columnNumber = 0;
          stringCharIterator.next();
        }
        if (!stringCharIterator.hasNext())
          break;
      }

      columnNumber += decodeVLQ(stringCharIterator);
      if (isSeparator(stringCharIterator.peek())) {
        this.#pushEntry(lineNumber, columnNumber, kNoSource, 0, 0, kNoName);
        continue;
      }

      sourceIndex += decodeVLQ(stringCharIterator);
      sourceLineNumber += decodeVLQ(stringCharIterator);
      sourceColumnNumber += decodeVLQ(stringCharIterator);

      let entryNameIndex = kNoName;
      if (!isSeparator(stringCharIterator.peek())) {
        nameIndex += decodeVLQ(stringCharIterator);
        if (nameIndex >= 0 && nameIndex < names.length) {
          entryNameIndex = nameBase + nameIndex;
        }
      }

      this.#pushEntry(
        lineNumber,
        columnNumber,
        sourceIndex >= 0 && sourceIndex < sources.length ?
          sourceBase + sourceIndex : kUnknownSource,
        sourceLineNumber,
        sourceColumnNumber,
        entryNameIndex,
      );
    }
  }
}

/**
 * Counts the segments in a mappings string, so entry storage can be
 * allocated once.
 * @param {string} mappings
 * @returns {number}
 */
function countSegments(mappings) {
  if (typeof mappings !== 'string') {
    return 0;
  }
  let count = 0;
  let inSegment = false;
  for (let i = 0; i < mappings.length; ++i) {
    const code = StringPrototypeCharCodeAt(mappings, i);
    if (code === kComma || code === kSemicolon) {
      inSegment = false;
    } else if (!inSegment) {
      inSegment = true;
      count++;
    }
  }
  return count;
}

/**
 * @param {string} char
 * @returns {boolean}
 */
function isSeparator(char) {
  return char === ',' || char === ';';
}

/**
 * @param {SourceMap.StringCharIterator} stringCharIterator
 * @returns {number}
 */
function decodeVLQ(stringCharIterator) {
  // Read unsigned value.
  let result = 0;
  let shift = 0;
  let digit;
  do {
    digit = base64Map[stringCharIterator.next()];
    result += (digit & VLQ_BASE_MASK) << shift;
    shift += VLQ_BASE_SHIFT;
  } while (digit & VLQ_CONTINUATION_MASK);

  // Fix the sign.
  const negative = result & 1;
  // Use unsigned right shift, so that the 32nd bit is properly shifted to the
  // 31st, and the 32nd becomes unset.
  result >>>= 1;
  if (!negative) {
    return result;
  }

  // We need to OR here to ensure the 32nd bit (the sign bit in an Int32) is
  // always set for negative numbers. If `result` were 1, (meaning `negate` is
  // true and all other bits were zeros), `result` would now be 0. But -0
  // doesn't flip the 32nd bit as intended. All other numbers will successfully
  // set the 32nd bit without issue, so doing this is a noop for them.
  return -result | (1 << 31);
}

/**
 * @param {SourceMapV3} payload
 * @returns {SourceMapV3}
 */
function cloneSourceMapV3(payload) {
  validateObject(payload, 'payload');
  payload = { ...payload };
  for (const key in payload) {
    if (ObjectPrototypeHasOwnProperty(payload, key) &&
        ArrayIsArray(payload[key])) {
      payload[key] = ObjectFreeze(ArrayPrototypeSlice(payload[key]));
    }
  }
  return ObjectFreeze(payload);
}

module.exports = {
  kFirstEntry,
  SourceMap,
};
