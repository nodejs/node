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
  Symbol,
  TypedArrayPrototypeSet,
  TypedArrayPrototypeSlice,
  TypedArrayPrototypeSubarray,
} = primordials;

const {
  validateInt32,
  validateObject,
} = require('internal/validators');
const {
  decodeGeneratedRanges,
  decodeOriginalScopes,
} = require('internal/source_map/scopes');
const {
  StringCharIterator,
  decodeSignedVLQ,
} = require('internal/source_map/vlq');

/**
 * A source map, or an index map when `sections` is present.
 * @see https://tc39.es/ecma426/#sec-source-map-format
 * @see https://tc39.es/ecma426/#sec-index-source-map
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-source-map-format
 * @typedef {object} SourceMapV3
 * @property {number} version The version field.
 * @property {string} [file] The file field.
 * @property {string} [sourceRoot] The sourceRoot field.
 * @property {Array<string|null>} [sources] The sources field.
 * @property {Array<string|null>} [sourcesContent] The sourcesContent field.
 * @property {string[]} [names] The names field.
 * @property {string} [mappings] The mappings field.
 * @property {number[]} [ignoreList] The ignoreList field.
 * @property {Array<string|null>} [scopes] The scopes field.
 * @property {string[]} [ranges] The ranges field.
 * @property {SourceMapV3Section[]} [sections] The sections field of an index
 *   map.
 */

/**
 * A section of an index map.
 * @see https://tc39.es/ecma426/#sec-index-source-map
 * @typedef {object} SourceMapV3Section
 * @property {{ line: number, column: number }} offset The offset field.
 * @property {SourceMapV3} map The map field.
 */

const kFirstEntry = Symbol('kFirstEntry');
const kScopes = Symbol('kScopes');

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
  // Names of each map, indexed by the entries through a single index space
  // that starts at #nameBases[i] for #nameTables[i]. Kept as is instead of
  // being copied into one array.
  #nameTables = [];
  #nameBases = [];
  #nameCount = 0;
  #sources = {};
  #sourceContentByURL = {};
  #lineLengths = undefined;
  // Decoded on first use.
  #scopes = undefined;

  /**
   * @param {SourceMapV3} payload
   */
  constructor(payload, { lineLengths } = { __proto__: null }) {
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
   * The decoded `scopes` and `ranges` fields.
   * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-DecodeIndexSourceMap
   * @returns {{ originalScopes: object[][], generatedRanges: object[] }}
   *   `originalScopes` holds the [[RootScopes]] of each source, and
   *   `generatedRanges` the [[Ranges]].
   */
  get [kScopes]() {
    if (this.#scopes === undefined) {
      const { sections } = this.#payload;
      if (sections) {
        // TODO(legendecas): Decode the ranges of index map sections once
        // DecodeIndexSourceMap produces [[Ranges]].
        // Refs: https://github.com/tc39/ecma426/issues/294
        const originalScopes = [];
        for (let i = 0; i < sections.length; ++i) {
          const decoded = decodeScopes(sections[i].map, originalScopes.length);
          // 9.b.xiv.2. For each Decoded Source Record _additionalSource_ of _decodedSection_.[[Sources]], do
          //   a. If _sourceMap_.[[Sources]] does not contain _additionalSource_, then
          //     i. Append _additionalSource_ to _sourceMap_.[[Sources]].
          for (let j = 0; j < decoded.originalScopes.length; ++j) {
            ArrayPrototypePush(originalScopes, decoded.originalScopes[j]);
          }
        }
        this.#scopes = { originalScopes, generatedRanges: [] };
      } else {
        this.#scopes = decodeScopes(this.#payload, 0);
      }
    }
    return this.#scopes;
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
        segments += estimateSegments(sections[i]?.map?.mappings);
      }
    } else {
      segments = estimateSegments(this.#payload.mappings);
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
      // estimateSegments(), which happens for malformed mappings.
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

  #nameAt(index) {
    const bases = this.#nameBases;
    // Find the last table starting at or before the index.
    let low = 0;
    let high = bases.length - 1;
    while (low < high) {
      const middle = (low + high + 1) >> 1;
      if (bases[middle] <= index) {
        low = middle;
      } else {
        high = middle - 1;
      }
    }
    return this.#nameTables[low][index - bases[low]];
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
      name: nameIndex === kNoName ? undefined : this.#nameAt(nameIndex),
    };
  }

  /**
   * @param {Array.<SourceMapV3Section>} sections
   */
  #parseSections = (sections) => {
    for (let i = 0; i < sections.length; ++i) {
      const section = sections[i];
      // The offsets seed the Int32Array entries below, so reject anything
      // that would otherwise be implicitly coerced.
      validateInt32(section.offset.line, 'section.offset.line', 0);
      validateInt32(section.offset.column, 'section.offset.column', 0);
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
    const nameBase = this.#nameCount;
    ArrayPrototypePush(this.#nameTables, names);
    ArrayPrototypePush(this.#nameBases, nameBase);
    this.#nameCount += names.length;

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

      columnNumber += decodeSignedVLQ(stringCharIterator);
      if (!stringCharIterator.hasNext() ||
          isSeparator(stringCharIterator.peek())) {
        this.#pushEntry(lineNumber, columnNumber, kNoSource, 0, 0, kNoName);
        continue;
      }

      sourceIndex += decodeSignedVLQ(stringCharIterator);
      sourceLineNumber += decodeSignedVLQ(stringCharIterator);
      sourceColumnNumber += decodeSignedVLQ(stringCharIterator);

      let entryNameIndex = kNoName;
      if (stringCharIterator.hasNext() &&
          !isSeparator(stringCharIterator.peek())) {
        nameIndex += decodeSignedVLQ(stringCharIterator);
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
 * DecodeSourceMap ( _json_: a JSON object, _baseURL_: an URL, ): a Decoded Source Map Record
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-DecodeSourceMap
 * @see https://tc39.es/ecma426/branch/proposal-scopes/#sec-DecodeSourceMapSources
 * @param {SourceMapV3} map
 * @param {number} sourceBase index of the first source of `map` among all
 *   sources of the source map
 * @returns {{ originalScopes: object[][], generatedRanges: object[] }}
 */
function decodeScopes(map, sourceBase) {
  // 10. Let _namesField_ be GetOptionalListOfStrings(_json_, *"names"*).
  const names = ArrayIsArray(map.names) ? map.names : [];
  // 11. Let _scopesField_ be GetOptionalListOfStrings(_json_, *"scopes"*).
  const scopesField = ArrayIsArray(map.scopes) ? map.scopes : [];
  // 13. Let _sources_ be DecodeSourceMapSources(_baseURL_, _sourceRootField_, _sourcesField_,
  //     _sourcesContentField_, _ignoreListField_, _namesField_, _scopesField_).
  const originalScopes = [];
  const flattenedScopes = [];
  for (let i = 0; i < map.sources.length; ++i) {
    const flattened = [];
    // DecodeSourceMapSources
    //   6.f. If _index_ < _encodedScopes_' length, then
    //     i. Let _decodedSourceScopes_ be DecodeSourceScopes(_encodedScopes_[_index_], _names_).
    //     ii. Set _decodedSource_.[[RootScopes]] to _decodedSourceScopes_.
    ArrayPrototypePush(originalScopes,
                       decodeOriginalScopes(scopesField[i], names, sourceBase + i, flattened));
    ArrayPrototypePush(flattenedScopes, flattened);
  }
  // 12. Let _rangesField_ be GetOptionalListOfStrings(_json_, *"ranges"*).
  const rangesField = ArrayIsArray(map.ranges) ? map.ranges : [];
  // 14. Let _ranges_ be DecodeGeneratedRanges(_rangesField_, _sources_, _namesField_).
  const generatedRanges =
    decodeGeneratedRanges(rangesField, flattenedScopes, names, sourceBase);
  return { originalScopes, generatedRanges };
}

/**
 * Estimates the number of segments in a mappings string, so entry storage
 * can be allocated up front without a separate pass over the string. A
 * segment takes four to five VLQ digits and a separator in typical maps.
 * #pushEntry grows the storage if the estimate turns out to be too low, and
 * #sortEntries trims it if it is too high.
 * @param {string} mappings
 * @returns {number}
 */
function estimateSegments(mappings) {
  if (typeof mappings !== 'string') {
    return 0;
  }
  return (mappings.length + 3) >> 2;
}

/**
 * @param {string} char
 * @returns {boolean}
 */
function isSeparator(char) {
  return char === ',' || char === ';';
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
  kScopes,
  SourceMap,
};
