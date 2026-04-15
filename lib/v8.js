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
  BigInt64Array,
  BigUint64Array,
  DataView,
  Error,
  Float32Array,
  Float64Array,
  Int16Array,
  Int32Array,
  Int8Array,
  JSONParse,
  ObjectKeys,
  ObjectPrototypeToString,
  ReflectGet,
  SymbolDispose,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray,
  globalThis: {
    Float16Array,
  },
} = primordials;

const { Buffer } = require('buffer');
const {
  validateFunction,
  validateObject,
  validateString,
  validateUint32,
  validateOneOf,
} = require('internal/validators');
const {
  Serializer,
  Deserializer,
} = internalBinding('serdes');
const {
  namespace: startupSnapshot,
} = require('internal/v8/startup_snapshot');

let profiler = {};
if (internalBinding('config').hasInspector) {
  profiler = internalBinding('profiler');
}

const assert = require('internal/assert');
const { inspect } = require('internal/util/inspect');
const { FastBuffer } = require('internal/buffer');
const { getValidatedPath } = require('internal/fs/utils');
const {
  createHeapSnapshotStream,
  triggerHeapSnapshot,
} = internalBinding('heap_utils');
const {
  HeapSnapshotStream,
  getHeapSnapshotOptions,
  queryObjects,
} = require('internal/heap_utils');
const promiseHooks = require('internal/promise_hooks');
const { getOptionValue } = require('internal/options');

/**
 * Generates a snapshot of the current V8 heap
 * and writes it to a JSON file.
 * @param {string} [filename]
 * @param {{
 *   exposeInternals?: boolean,
 *   exposeNumericValues?: boolean
 * }} [options]
 * @returns {string}
 */
function writeHeapSnapshot(filename, options) {
  if (filename !== undefined) {
    filename = getValidatedPath(filename);
  }
  const optionArray = getHeapSnapshotOptions(options);
  return triggerHeapSnapshot(filename, optionArray);
}

/**
 * Generates a snapshot of the current V8 heap
 * and returns a Readable Stream.
 * @param {{
 *   exposeInternals?: boolean,
 *   exposeNumericValues?: boolean
 * }} [options]
 * @returns {import('./stream.js').Readable}
 */
function getHeapSnapshot(options) {
  const optionArray = getHeapSnapshotOptions(options);
  const handle = createHeapSnapshotStream(optionArray);
  assert(handle);
  return new HeapSnapshotStream(handle);
}

// We need to get the buffer from the binding at the callsite since
// it's re-initialized after deserialization.
const binding = internalBinding('v8');

const {
  cachedDataVersionTag,
  setFlagsFromString: _setFlagsFromString,
  startCpuProfile: _startCpuProfile,
  stopCpuProfile: _stopCpuProfile,
  isStringOneByteRepresentation: _isStringOneByteRepresentation,
  updateHeapStatisticsBuffer,
  updateHeapSpaceStatisticsBuffer,
  updateHeapCodeStatisticsBuffer,
  setHeapSnapshotNearHeapLimit: _setHeapSnapshotNearHeapLimit,

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
  kTotalAllocatedBytes,

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

  heapStatisticsBuffer,
  heapCodeStatisticsBuffer,
  heapSpaceStatisticsBuffer,
  getCppHeapStatistics: _getCppHeapStatistics,
  detailLevel,

  startSamplingHeapProfiler: _startSamplingHeapProfiler,
  stopSamplingHeapProfiler: _stopSamplingHeapProfiler,
  getAllocationProfile: _getAllocationProfile,
  setHeapProfileLabelsStore: _setHeapProfileLabelsStore,
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

class SyncCPUProfileHandle {
  #id = null;
  #stopped = false;

  constructor(id) {
    this.#id = id;
  }

  stop() {
    if (this.#stopped) {
      return;
    }
    this.#stopped = true;
    return _stopCpuProfile(this.#id);
  };

  [SymbolDispose]() {
    this.stop();
  }
}

/**
 * Starting CPU Profile.
 * @returns {SyncCPUProfileHandle}
 */
function startCpuProfile() {
  const id = _startCpuProfile();
  return new SyncCPUProfileHandle(id);
}

/**
 * Return whether this string uses one byte as underlying representation or not.
 * @param {string} content
 * @returns {boolean}
 */
function isStringOneByteRepresentation(content) {
  validateString(content, 'content');
  return _isStringOneByteRepresentation(content);
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
  const buffer = heapStatisticsBuffer;

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
    external_memory: buffer[kExternalMemoryIndex],
    total_allocated_bytes: buffer[kTotalAllocatedBytes],
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
  const buffer = heapSpaceStatisticsBuffer;

  for (let i = 0; i < kNumberOfHeapSpaces; i++) {
    updateHeapSpaceStatisticsBuffer(i);
    heapSpaceStatistics[i] = {
      space_name: kHeapSpaces[i],
      space_size: buffer[kSpaceSizeIndex],
      space_used_size: buffer[kSpaceUsedSizeIndex],
      space_available_size: buffer[kSpaceAvailableSizeIndex],
      physical_space_size: buffer[kPhysicalSpaceSizeIndex],
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
  const buffer = heapCodeStatisticsBuffer;

  updateHeapCodeStatisticsBuffer();
  return {
    code_and_metadata_size: buffer[kCodeAndMetadataSizeIndex],
    bytecode_and_metadata_size: buffer[kBytecodeAndMetadataSizeIndex],
    external_script_source_size: buffer[kExternalScriptSourceSizeIndex],
    cpu_profiler_metadata_size: buffer[kCPUProfilerMetaDataSizeIndex],
  };
}

let heapSnapshotNearHeapLimitCallbackAdded = false;
function setHeapSnapshotNearHeapLimit(limit) {
  validateUint32(limit, 'limit', true);
  if (heapSnapshotNearHeapLimitCallbackAdded ||
      getOptionValue('--heapsnapshot-near-heap-limit') > 0
  ) {
    return;
  }
  heapSnapshotNearHeapLimitCallbackAdded = true;
  _setHeapSnapshotNearHeapLimit(limit);
}

const detailLevelDict = {
  __proto__: null,
  detailed: detailLevel.DETAILED,
  brief: detailLevel.BRIEF,
};

function getCppHeapStatistics(type = 'detailed') {
  validateOneOf(type, 'type', ['brief', 'detailed']);
  const result = _getCppHeapStatistics(detailLevelDict[type]);
  result.detail_level = type;
  return result;
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

function arrayBufferViewTypeToIndex(abView) {
  const type = ObjectPrototypeToString(abView);
  if (type === '[object Int8Array]') return 0;
  if (type === '[object Uint8Array]') return 1;
  if (type === '[object Uint8ClampedArray]') return 2;
  if (type === '[object Int16Array]') return 3;
  if (type === '[object Uint16Array]') return 4;
  if (type === '[object Int32Array]') return 5;
  if (type === '[object Uint32Array]') return 6;
  if (type === '[object Float32Array]') return 7;
  if (type === '[object Float64Array]') return 8;
  if (type === '[object DataView]') return 9;
  // Index 10 is FastBuffer.
  if (type === '[object BigInt64Array]') return 11;
  if (type === '[object BigUint64Array]') return 12;
  if (type === '[object Float16Array]') return 13;
  return -1;
}

function arrayBufferViewIndexToType(index) {
  if (index === 0) return Int8Array;
  if (index === 1) return Uint8Array;
  if (index === 2) return Uint8ClampedArray;
  if (index === 3) return Int16Array;
  if (index === 4) return Uint16Array;
  if (index === 5) return Int32Array;
  if (index === 6) return Uint32Array;
  if (index === 7) return Float32Array;
  if (index === 8) return Float64Array;
  if (index === 9) return DataView;
  if (index === 10) return FastBuffer;
  if (index === 11) return BigInt64Array;
  if (index === 12) return BigUint64Array;
  if (index === 13) return Float16Array;
  return undefined;
}

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
    // Keep track of how to handle different ArrayBufferViews. The default
    // Serializer for Node does not use the V8 methods for serializing those
    // objects because Node's `Buffer` objects use pooled allocation in many
    // cases, and their underlying `ArrayBuffer`s would show up in the
    // serialization. Because a) those may contain sensitive data and the user
    // may not be aware of that and b) they are often much larger than the
    // `Buffer` itself, custom serialization is applied.
    let i = 10;  // FastBuffer
    if (abView.constructor !== Buffer) {
      i = arrayBufferViewTypeToIndex(abView);
      if (i === -1) {
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
    const ctor = arrayBufferViewIndexToType(typeIndex);
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
    buffer_copy.set(new Uint8Array(this.buffer.buffer, this.buffer.byteOffset + byteOffset, byteLength));
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

class GCProfiler {
  #profiler = null;

  start() {
    if (!this.#profiler) {
      this.#profiler = new binding.GCProfiler();
      this.#profiler.start();
    }
  }

  stop() {
    if (this.#profiler) {
      const data = this.#profiler.stop();
      this.#profiler = null;
      return JSONParse(data);
    }
  }

  [SymbolDispose]() {
    this.stop();
  }
}

// --- Heap profile labels API ---
// Internal AsyncLocalStorage for propagating labels through async context.
// Requires --experimental-async-context-frame (Node 22) or Node 24+.
// Lazily initialized on first use so processes that never use heap profiling
// pay zero cost (no ALS instance, no async_hooks require).
let _heapProfileLabelsALS;

function ensureHeapProfileLabelsALS() {
  if (_heapProfileLabelsALS === undefined) {
    // When V8_HEAP_PROFILER_SAMPLE_LABELS is compiled out, the C++ binding
    // for _setHeapProfileLabelsStore is not registered — labels are a no-op.
    if (typeof _setHeapProfileLabelsStore !== 'function') return;
    const { AsyncLocalStorage } = require('async_hooks');
    _heapProfileLabelsALS = new AsyncLocalStorage();
    // The ALS instance is passed to C++ as the key used to look up labels in
    // the AsyncContextFrame map (via ContinuationPreservedEmbedderData).
    // This relies on the async-context-frame implementation storing ALS
    // instances as Map keys — if that internal representation changes, the
    // C++ label-resolution code in node_v8.cc must be updated to match.
    _setHeapProfileLabelsStore(_heapProfileLabelsALS);
  }
  return _heapProfileLabelsALS;
}

/**
 * Convert a labels object to a flat array [key1, val1, key2, val2, ...].
 * Pre-flattened at label-set time (not per allocation/sample) because the
 * C++ callback runs in GetAllocationProfile() BEFORE BuildSamples() where
 * it resolves labels from CPED captured at allocation time.
 * @param {Record<string, string>} labels
 * @returns {string[]}
 */
function labelsToFlat(labels) {
  const keys = ObjectKeys(labels);
  const len = keys.length;
  const flat = new Array(len * 2);
  for (let i = 0; i < len; i++) {
    const key = keys[i];
    const val = ReflectGet(labels, key);
    validateString(val, `labels.${key}`);
    flat[i * 2] = key;
    flat[i * 2 + 1] = val;
  }
  return flat;
}

/**
 * Starts the V8 sampling heap profiler.
 * @param {number} [sampleInterval] - Average bytes between samples (default 512 KB).
 * @param {number} [stackDepth] - Maximum stack depth for samples (default 16).
 * @param {object} [options] - Options object.
 * @param {boolean} [options.includeCollectedObjects] - If true, retain
 *   samples for objects collected by GC (allocation-rate mode).
 */
function startSamplingHeapProfiler(sampleInterval, stackDepth, options) {
  if (sampleInterval !== undefined) validateUint32(sampleInterval, 'sampleInterval', true);
  if (stackDepth !== undefined) validateUint32(stackDepth, 'stackDepth');
  if (options !== undefined) validateObject(options, 'options');
  return _startSamplingHeapProfiler(sampleInterval, stackDepth, options);
}

/**
 * Runs `fn` with the given heap profile labels active. Labels propagate
 * across `await` boundaries via AsyncLocalStorage. If `fn` returns a
 * Promise, labels remain active until the Promise settles.
 *
 * @param {Record<string, string>} labels
 * @param {Function} fn
 * @returns {*} The return value of `fn`.
 */
function withHeapProfileLabels(labels, fn) {
  validateObject(labels, 'labels');
  validateFunction(fn, 'fn');
  // Store the flat [key1, val1, key2, val2, ...] array in ALS.
  // Conversion happens once at label-set time (not per allocation).
  // The C++ callback resolves labels from CPED in GetAllocationProfile()
  // before BuildSamples() — pre-flattening avoids V8 Object property
  // access during label resolution.
  const flat = labelsToFlat(labels);
  const als = ensureHeapProfileLabelsALS();
  // When labels are compiled out, still run the callback — just without
  // label tracking.
  if (als === undefined) return fn();
  return als.run(flat, fn);
}

/**
 * Sets heap profile labels for the current async scope using
 * `enterWith` semantics. Labels persist until overwritten or the
 * async scope ends. Useful for frameworks (e.g. Hapi) where the
 * handler runs after the extension returns.
 *
 * @param {Record<string, string>} labels
 */
function setHeapProfileLabels(labels) {
  validateObject(labels, 'labels');
  const flat = labelsToFlat(labels);
  const als = ensureHeapProfileLabelsALS();
  // When labels are compiled out, setHeapProfileLabels is a no-op.
  if (als === undefined) return;
  als.enterWith(flat);
}

module.exports = {
  cachedDataVersionTag,
  getHeapSnapshot,
  getHeapStatistics,
  getHeapSpaceStatistics,
  getHeapCodeStatistics,
  getCppHeapStatistics,
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
  queryObjects,
  startupSnapshot,
  setHeapSnapshotNearHeapLimit,
  GCProfiler,
  isStringOneByteRepresentation,
  startCpuProfile,
  startSamplingHeapProfiler,
  stopSamplingHeapProfiler: _stopSamplingHeapProfiler,
  getAllocationProfile: _getAllocationProfile,
  withHeapProfileLabels,
  setHeapProfileLabels,
};
