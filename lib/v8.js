// Copyright (c) 2014, StrongLoop Inc.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

'use strict';

const {
  Array,
  ArrayBuffer,
  ArrayPrototypeForEach,
  ArrayPrototypePush,
  DataView,
  Error,
  Float32Array,
  Float64Array,
  Int16Array,
  Int32Array,
  Int8Array,
  ObjectPrototypeToString,
  SafeMap,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray,
} = primordials;

const { Buffer } = require('buffer');
const { validateString } = require('internal/validators');
const {
  Serializer,
  Deserializer
} = internalBinding('serdes');

let profiler = {};
if (internalBinding('config').hasInspector) {
  profiler = internalBinding('profiler');
}

const assert = require('internal/assert');
const { copy } = internalBinding('buffer');
const { inspect } = require('internal/util/inspect');
const { FastBuffer } = require('internal/buffer');
const { getValidatedPath } = require('internal/fs/utils');
const { toNamespacedPath } = require('path');
const {
  createHeapSnapshotStream,
  triggerHeapSnapshot
} = internalBinding('heap_utils');
const { HeapSnapshotStream } = require('internal/heap_utils');
const promiseHooks = require('internal/promise_hooks');

/**
 * Generates a snapshot of the current V8 heap
 * and writes it to a JSON file.
 * @param {string} [filename]
 * @returns {string}
 */
function writeHeapSnapshot(filename) {
  if (filename !== undefined) {
    filename = getValidatedPath(filename);
    filename = toNamespacedPath(filename);
  }
  return triggerHeapSnapshot(filename);
}

/**
 * Generates a snapshot of the current V8 heap
 * and returns a Readable Stream.
 * @returns {import('./stream.js').Readable}
 */
function getHeapSnapshot() {
  const handle = createHeapSnapshotStream();
  assert(handle);
  return new HeapSnapshotStream(handle);
}

// We need to get the buffer from the binding at the callsite since
// it's re-initialized after deserialization.
const binding = internalBinding('v8');

const {
  cachedDataVersionTag,
  setFlagsFromString: _setFlagsFromString,
  updateHeapStatisticsBuffer,
  updateHeapSpaceStatisticsBuffer,
  updateHeapCodeStatisticsBuffer,

  // Properties for heap statistics buffer extraction.
  kTotalHeapSizeIndex,
  kTotalHeapSizeExecutableIndex,
  kTotalPhysicalSizeIndex,
  kTotalAvailableSize,
  kUsedHeapSizeIndex,
  kHeapSizeLimitIndex,
  kDoesZapGarbageIndex,
  kMallocedMemoryIndex,
  kPeakMallocedMemoryIndex,
  kNumberOfNativeContextsIndex,
  kNumberOfDetachedContextsIndex,
  kTotalGlobalHandlesSizeIndex,
  kUsedGlobalHandlesSizeIndex,
  kExternalMemoryIndex,

  // Properties for heap spaces statistics buffer extraction.
  kHeapSpaces,
  kSpaceSizeIndex,
  kSpaceUsedSizeIndex,
  kSpaceAvailableSizeIndex,
  kPhysicalSpaceSizeIndex,

  // Properties for heap code statistics buffer extraction.
  kCodeAndMetadataSizeIndex,
  kBytecodeAndMetadataSizeIndex,
  kExternalScriptSourceSizeIndex,
  kCPUProfilerMetaDataSizeIndex,
} = binding;

const kNumberOfHeapSpaces = kHeapSpaces.length;

/**
 * Sets V8 command-line flags.
 * @param {string} flags
 * @returns {void}
 */
function setFlagsFromString(flags) {
  validateString(flags, 'flags');
  _setFlagsFromString(flags);
}

/**
 * Gets the current V8 heap statistics.
 * @returns {{
 *   total_heap_size: number;
 *   total_heap_size_executable: number;
 *   total_physical_size: number;
 *   total_available_size: number;
 *   used_heap_size: number;
 *   heap_size_limit: number;
 *   malloced_memory: number;
 *   peak_malloced_memory: number;
 *   does_zap_garbage: number;
 *   number_of_native_contexts: number;
 *   number_of_detached_contexts: number;
 *   }}
 */
function getHeapStatistics() {
  const buffer = binding.heapStatisticsBuffer;

  updateHeapStatisticsBuffer();

  return {
    total_heap_size: buffer[kTotalHeapSizeIndex],
    total_heap_size_executable: buffer[kTotalHeapSizeExecutableIndex],
    total_physical_size: buffer[kTotalPhysicalSizeIndex],
    total_available_size: buffer[kTotalAvailableSize],
    used_heap_size: buffer[kUsedHeapSizeIndex],
    heap_size_limit: buffer[kHeapSizeLimitIndex],
    malloced_memory: buffer[kMallocedMemoryIndex],
    peak_malloced_memory: buffer[kPeakMallocedMemoryIndex],
    does_zap_garbage: buffer[kDoesZapGarbageIndex],
    number_of_native_contexts: buffer[kNumberOfNativeContextsIndex],
    number_of_detached_contexts: buffer[kNumberOfDetachedContextsIndex],
    total_global_handles_size: buffer[kTotalGlobalHandlesSizeIndex],
    used_global_handles_size: buffer[kUsedGlobalHandlesSizeIndex],
    external_memory: buffer[kExternalMemoryIndex]
  };
}

/**
 * Gets the current V8 heap space statistics.
 * @returns {{
 *   space_name: string;
 *   space_size: number;
 *   space_used_size: number;
 *   space_available_size: number;
 *   physical_space_size: number;
 *   }[]}
 */
function getHeapSpaceStatistics() {
  const heapSpaceStatistics = new Array(kNumberOfHeapSpaces);
  const buffer = binding.heapSpaceStatisticsBuffer;

  for (let i = 0; i < kNumberOfHeapSpaces; i++) {
    updateHeapSpaceStatisticsBuffer(i);
    heapSpaceStatistics[i] = {
      space_name: kHeapSpaces[i],
      space_size: buffer[kSpaceSizeIndex],
      space_used_size: buffer[kSpaceUsedSizeIndex],
      space_available_size: buffer[kSpaceAvailableSizeIndex],
      physical_space_size: buffer[kPhysicalSpaceSizeIndex]
    };
  }

  return heapSpaceStatistics;
}

/**
 * Gets the current V8 heap code statistics.
 * @returns {{
 *   code_and_metadata_size: number;
 *   bytecode_and_metadata_size: number;
 *   external_script_source_size: number;
 *   cpu_profiler_metadata_size: number;
 *   }}
 */
function getHeapCodeStatistics() {
  const buffer = binding.heapCodeStatisticsBuffer;

  updateHeapCodeStatisticsBuffer();
  return {
    code_and_metadata_size: buffer[kCodeAndMetadataSizeIndex],
    bytecode_and_metadata_size: buffer[kBytecodeAndMetadataSizeIndex],
    external_script_source_size: buffer[kExternalScriptSourceSizeIndex],
    cpu_profiler_metadata_size: buffer[kCPUProfilerMetaDataSizeIndex],
  };
}

/* V8 serialization API */

/* JS methods for the base objects */
Serializer.prototype._getDataCloneError = Error;

/**
 * Reads raw bytes from the deserializer's internal buffer.
 * @param {number} length
 * @returns {Buffer}
 */
Deserializer.prototype.readRawBytes = function readRawBytes(length) {
  const offset = this._readRawBytes(length);
  // `this.buffer` can be a Buffer or a plain Uint8Array, so just calling
  // `.slice()` doesn't work.
  return new FastBuffer(this.buffer.buffer,
                        this.buffer.byteOffset + offset,
                        length);
};

/* Keep track of how to handle different ArrayBufferViews.
 * The default Serializer for Node does not use the V8 methods for serializing
 * those objects because Node's `Buffer` objects use pooled allocation in many
 * cases, and their underlying `ArrayBuffer`s would show up in the
 * serialization. Because a) those may contain sensitive data and the user
 * may not be aware of that and b) they are often much larger than the `Buffer`
 * itself, custom serialization is applied. */
const arrayBufferViewTypes = [Int8Array, Uint8Array, Uint8ClampedArray,
                              Int16Array, Uint16Array, Int32Array, Uint32Array,
                              Float32Array, Float64Array, DataView];

const arrayBufferViewTypeToIndex = new SafeMap();

{
  const dummy = new ArrayBuffer();
  ArrayPrototypeForEach(arrayBufferViewTypes, (ctor, i) => {
    const tag = ObjectPrototypeToString(new ctor(dummy));
    arrayBufferViewTypeToIndex.set(tag, i);
  });
}

const bufferConstructorIndex =
  ArrayPrototypePush(arrayBufferViewTypes, FastBuffer) - 1;

class DefaultSerializer extends Serializer {
  constructor() {
    super();

    this._setTreatArrayBufferViewsAsHostObjects(true);
  }

  /**
   * Used to write some kind of host object, i.e. an
   * object that is created by native C++ bindings.
   * @param {object} abView
   * @returns {void}
   */
  _writeHostObject(abView) {
    let i = 0;
    if (abView.constructor === Buffer) {
      i = bufferConstructorIndex;
    } else {
      const tag = ObjectPrototypeToString(abView);
      i = arrayBufferViewTypeToIndex.get(tag);

      if (i === undefined) {
        throw new this._getDataCloneError(
          `Unserializable host object: ${inspect(abView)}`);
      }
    }
    this.writeUint32(i);
    this.writeUint32(abView.byteLength);
    this.writeRawBytes(new Uint8Array(abView.buffer,
                                      abView.byteOffset,
                                      abView.byteLength));
  }
}

class DefaultDeserializer extends Deserializer {
  /**
   * Used to read some kind of host object, i.e. an
   * object that is created by native C++ bindings.
   * @returns {any}
   */
  _readHostObject() {
    const typeIndex = this.readUint32();
    const ctor = arrayBufferViewTypes[typeIndex];
    const byteLength = this.readUint32();
    const byteOffset = this._readRawBytes(byteLength);
    const BYTES_PER_ELEMENT = ctor.BYTES_PER_ELEMENT || 1;

    const offset = this.buffer.byteOffset + byteOffset;
    if (offset % BYTES_PER_ELEMENT === 0) {
      return new ctor(this.buffer.buffer,
                      offset,
                      byteLength / BYTES_PER_ELEMENT);
    }
    // Copy to an aligned buffer first.
    const buffer_copy = Buffer.allocUnsafe(byteLength);
    copy(this.buffer, buffer_copy, 0, byteOffset, byteOffset + byteLength);
    return new ctor(buffer_copy.buffer,
                    buffer_copy.byteOffset,
                    byteLength / BYTES_PER_ELEMENT);
  }
}

/**
 * Uses a `DefaultSerializer` to serialize `value`
 * into a buffer.
 * @param {any} value
 * @returns {Buffer}
 */
function serialize(value) {
  const ser = new DefaultSerializer();
  ser.writeHeader();
  ser.writeValue(value);
  return ser.releaseBuffer();
}

/**
 * Uses a `DefaultDeserializer` with default options
 * to read a JavaScript value from a buffer.
 * @param {Buffer | TypedArray | DataView} buffer
 * @returns {any}
 */
function deserialize(buffer) {
  const der = new DefaultDeserializer(buffer);
  der.readHeader();
  return der.readValue();
}

module.exports = {
  cachedDataVersionTag,
  getHeapSnapshot,
  getHeapStatistics,
  getHeapSpaceStatistics,
  getHeapCodeStatistics,
  setFlagsFromString,
  Serializer,
  Deserializer,
  DefaultSerializer,
  DefaultDeserializer,
  deserialize,
  takeCoverage: profiler.takeCoverage,
  stopCoverage: profiler.stopCoverage,
  serialize,
  writeHeapSnapshot,
  promiseHooks,
};
