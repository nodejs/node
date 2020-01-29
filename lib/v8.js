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
  Error,
  Float32Array,
  Float64Array,
  Int16Array,
  Int32Array,
  Int8Array,
  Map,
  ObjectPrototypeToString,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray,
} = primordials;

const { Buffer } = require('buffer');
const { validateString } = require('internal/validators');
const {
  Serializer: _Serializer,
  Deserializer: _Deserializer
} = internalBinding('serdes');
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

function writeHeapSnapshot(filename) {
  if (filename !== undefined) {
    filename = getValidatedPath(filename);
    filename = toNamespacedPath(filename);
  }
  return triggerHeapSnapshot(filename);
}

function getHeapSnapshot() {
  const handle = createHeapSnapshotStream();
  assert(handle);
  return new HeapSnapshotStream(handle);
}

// Calling exposed c++ functions directly throws exception as it expected to be
// called with new operator and caused an assert to fire.
// Creating JS wrapper so that it gets caught at JS layer.
class Serializer extends _Serializer { }

class Deserializer extends _Deserializer { }

const {
  cachedDataVersionTag,
  setFlagsFromString: _setFlagsFromString,
  heapStatisticsArrayBuffer,
  heapSpaceStatisticsArrayBuffer,
  heapCodeStatisticsArrayBuffer,
  updateHeapStatisticsArrayBuffer,
  updateHeapSpaceStatisticsArrayBuffer,
  updateHeapCodeStatisticsArrayBuffer,

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

  // Properties for heap spaces statistics buffer extraction.
  kHeapSpaces,
  kHeapSpaceStatisticsPropertiesCount,
  kSpaceSizeIndex,
  kSpaceUsedSizeIndex,
  kSpaceAvailableSizeIndex,
  kPhysicalSpaceSizeIndex,

  // Properties for heap code statistics buffer extraction.
  kCodeAndMetadataSizeIndex,
  kBytecodeAndMetadataSizeIndex,
  kExternalScriptSourceSizeIndex
} = internalBinding('v8');

const kNumberOfHeapSpaces = kHeapSpaces.length;

const heapStatisticsBuffer =
    new Float64Array(heapStatisticsArrayBuffer);

const heapSpaceStatisticsBuffer =
    new Float64Array(heapSpaceStatisticsArrayBuffer);

const heapCodeStatisticsBuffer =
    new Float64Array(heapCodeStatisticsArrayBuffer);

function setFlagsFromString(flags) {
  validateString(flags, 'flags');
  _setFlagsFromString(flags);
}

function getHeapStatistics() {
  const buffer = heapStatisticsBuffer;

  updateHeapStatisticsArrayBuffer();

  return {
    'total_heap_size': buffer[kTotalHeapSizeIndex],
    'total_heap_size_executable': buffer[kTotalHeapSizeExecutableIndex],
    'total_physical_size': buffer[kTotalPhysicalSizeIndex],
    'total_available_size': buffer[kTotalAvailableSize],
    'used_heap_size': buffer[kUsedHeapSizeIndex],
    'heap_size_limit': buffer[kHeapSizeLimitIndex],
    'malloced_memory': buffer[kMallocedMemoryIndex],
    'peak_malloced_memory': buffer[kPeakMallocedMemoryIndex],
    'does_zap_garbage': buffer[kDoesZapGarbageIndex],
    'number_of_native_contexts': buffer[kNumberOfNativeContextsIndex],
    'number_of_detached_contexts': buffer[kNumberOfDetachedContextsIndex]
  };
}

function getHeapSpaceStatistics() {
  const heapSpaceStatistics = new Array(kNumberOfHeapSpaces);
  const buffer = heapSpaceStatisticsBuffer;
  updateHeapSpaceStatisticsArrayBuffer();

  for (let i = 0; i < kNumberOfHeapSpaces; i++) {
    const propertyOffset = i * kHeapSpaceStatisticsPropertiesCount;
    heapSpaceStatistics[i] = {
      space_name: kHeapSpaces[i],
      space_size: buffer[propertyOffset + kSpaceSizeIndex],
      space_used_size: buffer[propertyOffset + kSpaceUsedSizeIndex],
      space_available_size: buffer[propertyOffset + kSpaceAvailableSizeIndex],
      physical_space_size: buffer[propertyOffset + kPhysicalSpaceSizeIndex]
    };
  }

  return heapSpaceStatistics;
}

function getHeapCodeStatistics() {
  const buffer = heapCodeStatisticsBuffer;

  updateHeapCodeStatisticsArrayBuffer();
  return {
    'code_and_metadata_size': buffer[kCodeAndMetadataSizeIndex],
    'bytecode_and_metadata_size': buffer[kBytecodeAndMetadataSizeIndex],
    'external_script_source_size': buffer[kExternalScriptSourceSizeIndex]
  };
}

/* V8 serialization API */

/* JS methods for the base objects */
Serializer.prototype._getDataCloneError = Error;

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

const arrayBufferViewTypeToIndex = new Map();

{
  const dummy = new ArrayBuffer();
  for (const [i, ctor] of arrayBufferViewTypes.entries()) {
    const tag = ObjectPrototypeToString(new ctor(dummy));
    arrayBufferViewTypeToIndex.set(tag, i);
  }
}

const bufferConstructorIndex = arrayBufferViewTypes.push(FastBuffer) - 1;

class DefaultSerializer extends Serializer {
  constructor() {
    super();

    this._setTreatArrayBufferViewsAsHostObjects(true);
  }

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
    } else {
      // Copy to an aligned buffer first.
      const buffer_copy = Buffer.allocUnsafe(byteLength);
      copy(this.buffer, buffer_copy, 0, byteOffset, byteOffset + byteLength);
      return new ctor(buffer_copy.buffer,
                      buffer_copy.byteOffset,
                      byteLength / BYTES_PER_ELEMENT);
    }
  }
}

function serialize(value) {
  const ser = new DefaultSerializer();
  ser.writeHeader();
  ser.writeValue(value);
  return ser.releaseBuffer();
}

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
  serialize,
  writeHeapSnapshot,
};
