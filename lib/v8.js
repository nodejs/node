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

const Buffer = require('buffer').Buffer;

const v8binding = process.binding('v8');
const serdesBinding = process.binding('serdes');
const bufferBinding = process.binding('buffer');

const { objectToString } = require('internal/util');
const { FastBuffer } = require('internal/buffer');

// Properties for heap statistics buffer extraction.
const heapStatisticsBuffer =
    new Float64Array(v8binding.heapStatisticsArrayBuffer);
const kTotalHeapSizeIndex = v8binding.kTotalHeapSizeIndex;
const kTotalHeapSizeExecutableIndex = v8binding.kTotalHeapSizeExecutableIndex;
const kTotalPhysicalSizeIndex = v8binding.kTotalPhysicalSizeIndex;
const kTotalAvailableSize = v8binding.kTotalAvailableSize;
const kUsedHeapSizeIndex = v8binding.kUsedHeapSizeIndex;
const kHeapSizeLimitIndex = v8binding.kHeapSizeLimitIndex;
const kDoesZapGarbageIndex = v8binding.kDoesZapGarbageIndex;
const kMallocedMemoryIndex = v8binding.kMallocedMemoryIndex;
const kPeakMallocedMemoryIndex = v8binding.kPeakMallocedMemoryIndex;

// Properties for heap space statistics buffer extraction.
const heapSpaceStatisticsBuffer =
    new Float64Array(v8binding.heapSpaceStatisticsArrayBuffer);
const kHeapSpaces = v8binding.kHeapSpaces;
const kNumberOfHeapSpaces = kHeapSpaces.length;
const kHeapSpaceStatisticsPropertiesCount =
    v8binding.kHeapSpaceStatisticsPropertiesCount;
const kSpaceSizeIndex = v8binding.kSpaceSizeIndex;
const kSpaceUsedSizeIndex = v8binding.kSpaceUsedSizeIndex;
const kSpaceAvailableSizeIndex = v8binding.kSpaceAvailableSizeIndex;
const kPhysicalSpaceSizeIndex = v8binding.kPhysicalSpaceSizeIndex;

exports.getHeapStatistics = function() {
  const buffer = heapStatisticsBuffer;

  v8binding.updateHeapStatisticsArrayBuffer();

  return {
    'total_heap_size': buffer[kTotalHeapSizeIndex],
    'total_heap_size_executable': buffer[kTotalHeapSizeExecutableIndex],
    'total_physical_size': buffer[kTotalPhysicalSizeIndex],
    'total_available_size': buffer[kTotalAvailableSize],
    'used_heap_size': buffer[kUsedHeapSizeIndex],
    'heap_size_limit': buffer[kHeapSizeLimitIndex],
    'malloced_memory': buffer[kMallocedMemoryIndex],
    'peak_malloced_memory': buffer[kPeakMallocedMemoryIndex],
    'does_zap_garbage': buffer[kDoesZapGarbageIndex]
  };
};

exports.cachedDataVersionTag = v8binding.cachedDataVersionTag;
exports.setFlagsFromString = v8binding.setFlagsFromString;

exports.getHeapSpaceStatistics = function() {
  const heapSpaceStatistics = new Array(kNumberOfHeapSpaces);
  const buffer = heapSpaceStatisticsBuffer;
  v8binding.updateHeapSpaceStatisticsArrayBuffer();

  for (var i = 0; i < kNumberOfHeapSpaces; i++) {
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
};

/* V8 serialization API */

const Serializer = exports.Serializer = serdesBinding.Serializer;
const Deserializer = exports.Deserializer = serdesBinding.Deserializer;

/* JS methods for the base objects */
Serializer.prototype._getDataCloneError = Error;

Deserializer.prototype.readRawBytes = function(length) {
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
    const tag = objectToString(new ctor(dummy));
    arrayBufferViewTypeToIndex.set(tag, i);
  }
}

const bufferConstructorIndex = arrayBufferViewTypes.push(Buffer) - 1;

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
      const tag = objectToString(abView);
      i = arrayBufferViewTypeToIndex.get(tag);

      if (i === undefined) {
        throw this._getDataCloneError(`Unknown host object type: ${tag}`);
      }
    }
    this.writeUint32(i);
    this.writeUint32(abView.byteLength);
    this.writeRawBytes(new Uint8Array(abView.buffer,
                                      abView.byteOffset,
                                      abView.byteLength));
  }
}

exports.DefaultSerializer = DefaultSerializer;

class DefaultDeserializer extends Deserializer {
  constructor(buffer) {
    super(buffer);
  }

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
      const copy = Buffer.allocUnsafe(byteLength);
      bufferBinding.copy(this.buffer, copy, 0,
                         byteOffset, byteOffset + byteLength);
      return new ctor(copy.buffer,
                      copy.byteOffset,
                      byteLength / BYTES_PER_ELEMENT);
    }
  }
}

exports.DefaultDeserializer = DefaultDeserializer;

exports.serialize = function serialize(value) {
  const ser = new DefaultSerializer();
  ser.writeHeader();
  ser.writeValue(value);
  return ser.releaseBuffer();
};

exports.deserialize = function deserialize(buffer) {
  const der = new DefaultDeserializer(buffer);
  der.readHeader();
  return der.readValue();
};
