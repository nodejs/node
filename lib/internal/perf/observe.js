'use strict';

const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  Error,
  MathMax,
  MathMin,
  ObjectDefineProperties,
  ObjectFreeze,
  SafeArrayPrototypePushApply,
  SafeMap,
  SafeSet,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  constants: {
    NODE_PERFORMANCE_ENTRY_TYPE_GC,
    NODE_PERFORMANCE_ENTRY_TYPE_HTTP2,
    NODE_PERFORMANCE_ENTRY_TYPE_HTTP,
    NODE_PERFORMANCE_ENTRY_TYPE_NET,
    NODE_PERFORMANCE_ENTRY_TYPE_DNS,
    NODE_PERFORMANCE_ENTRY_TYPE_QUIC,
  },
  observerCounts,
  setupObservers,
  updateGarbageCollectionTracking,
} = internalBinding('performance');

const {
  isPerformanceEntry,
  createPerformanceNodeEntry,
  kName,
} = require('internal/perf/performance_entry');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_MISSING_ARGS,
  },
} = require('internal/errors');

const {
  validateFunction,
  validateObject,
  validateThisInternalField,
} = require('internal/validators');

const {
  customInspectSymbol: kInspect,
  lazyDOMException,
  kEmptyObject,
  kEnumerableProperty,
} = require('internal/util');

const {
  setImmediate,
} = require('timers');

const { inspect } = require('util');

const { now } = require('internal/perf/utils');

const kBuffer = Symbol('kBuffer');
const kDispatch = Symbol('kDispatch');
const kMaybeBuffer = Symbol('kMaybeBuffer');

const kTypeSingle = 0;
const kTypeMultiple = 1;

const kSupportedEntryTypes = ObjectFreeze([
  'dns',
  'function',
  'gc',
  'http',
  'http2',
  'mark',
  'measure',
  'net',
  'quic',
  'resource',
]);

// Performance timeline entry Buffers
const kEntryBufferIndexThreshold = 64;

class EntryBuffer {
  entries = [];
  byName = null;
  size = 0;
  dead = 0;
  indexedLength = 0;
  probes = 0;

  #syncIndex() {
    let byName = this.byName;
    if (byName === null) {
      byName = new SafeMap();
      this.byName = byName;
      this.indexedLength = 0;
    }
    const entries = this.entries;
    const len = entries.length;
    for (let i = this.indexedLength; i < len; i++) {
      const name = entries[i][kName];
      const existing = byName.get(name);
      if (existing === undefined) {
        byName.set(name, i);
      } else if (typeof existing === 'number') {
        byName.set(name, [existing, i]);
      } else {
        existing[existing.length] = i;
      }
    }
    this.indexedLength = len;
  }

  #compact() {
    const entries = this.entries;
    const compacted = [];
    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i];
      if (entry !== undefined) {
        compacted[compacted.length] = entry;
      }
    }
    this.entries = compacted;
    this.size = compacted.length;
    this.dead = 0;
    this.byName = null;
    this.indexedLength = 0;
    this.probes = 0;
  }

  push(entry) {
    const entries = this.entries;
    entries[entries.length] = entry;
    return ++this.size;
  }

  clear(name) {
    if (name === undefined) {
      this.entries = [];
      this.byName = null;
      this.size = 0;
      this.dead = 0;
      this.indexedLength = 0;
      this.probes = 0;
      return;
    }
    if (this.byName === null && (this.size < kEntryBufferIndexThreshold || this.probes === 0)) {
      const wasLarge = this.size >= kEntryBufferIndexThreshold;
      this.entries = ArrayPrototypeFilter(this.entries, (entry) => entry[kName] !== name);
      this.size = this.entries.length;
      this.probes = wasLarge && this.size >= kEntryBufferIndexThreshold ? 1 : 0;
      return;
    }
    if (this.byName === null || this.indexedLength < this.entries.length) {
      this.#syncIndex();
    }
    const existing = this.byName.get(name);
    if (existing !== undefined) {
      if (typeof existing === 'number') {
        this.entries[existing] = undefined;
        this.size--;
        this.dead++;
      } else {
        for (let i = 0; i < existing.length; i++) {
          this.entries[existing[i]] = undefined;
        }
        this.size -= existing.length;
        this.dead += existing.length;
      }
      this.byName.delete(name);
      if (this.size === 0) {
        this.entries = [];
        this.byName = null;
        this.dead = 0;
        this.indexedLength = 0;
        this.probes = 0;
      } else if (this.dead >= this.size && this.entries.length >= kEntryBufferIndexThreshold * 2) {
        this.#compact();
      }
    }
  }

  getByName(name) {
    if (this.byName === null && (this.size < kEntryBufferIndexThreshold || this.probes === 0)) {
      if (this.size >= kEntryBufferIndexThreshold) {
        this.probes = 1;
      }
      const out = ArrayPrototypeFilter(this.entries, (entry) => entry[kName] === name);
      return out.length > 1 ? ArrayPrototypeSort(out, performanceObserverSorter) : out;
    }
    if (this.byName === null || this.indexedLength < this.entries.length) {
      this.#syncIndex();
    }
    const existing = this.byName.get(name);
    if (existing === undefined) return [];
    if (typeof existing === 'number') return [this.entries[existing]];
    const out = [];
    for (let i = 0; i < existing.length; i++) {
      out[i] = this.entries[existing[i]];
    }
    return ArrayPrototypeSort(out, performanceObserverSorter);
  }

  getAllSorted() {
    let out;
    if (this.dead === 0) {
      out = ArrayPrototypeSlice(this.entries);
    } else {
      out = [];
      const entries = this.entries;
      for (let i = 0; i < entries.length; i++) {
        const entry = entries[i];
        if (entry !== undefined) {
          out[out.length] = entry;
        }
      }
    }
    return out.length > 1 ? ArrayPrototypeSort(out, performanceObserverSorter) : out;
  }

  appendByNameTo(name, out) {
    if (this.byName === null && (this.size < kEntryBufferIndexThreshold || this.probes === 0)) {
      if (this.size >= kEntryBufferIndexThreshold) {
        this.probes = 1;
      }
      const entries = this.entries;
      for (let i = 0; i < entries.length; i++) {
        if (entries[i][kName] === name) {
          out[out.length] = entries[i];
        }
      }
      return;
    }
    if (this.byName === null || this.indexedLength < this.entries.length) {
      this.#syncIndex();
    }
    const existing = this.byName.get(name);
    if (existing === undefined) return;
    if (typeof existing === 'number') {
      out[out.length] = this.entries[existing];
      return;
    }
    for (let i = 0; i < existing.length; i++) {
      out[out.length] = this.entries[existing[i]];
    }
  }

  appendAllTo(out) {
    const entries = this.entries;
    if (this.dead === 0) {
      for (let i = 0; i < entries.length; i++) {
        out[out.length] = entries[i];
      }
      return;
    }
    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i];
      if (entry !== undefined) {
        out[out.length] = entry;
      }
    }
  }
}

const markEntryBuffer = new EntryBuffer();
const measureEntryBuffer = new EntryBuffer();
let resourceTimingBuffer = [];
let resourceTimingSecondaryBuffer = [];
const kPerformanceEntryBufferWarnSize = 1e6;
// https://www.w3.org/TR/timing-entrytypes-registry/#registry
// Default buffer limit for resource timing entries.
let resourceTimingBufferSizeLimit = 250;
let dispatchBufferFull;
let resourceTimingBufferFullPending = false;

const kClearPerformanceEntryBuffers = ObjectFreeze({
  'mark': 'performance.clearMarks',
  'measure': 'performance.clearMeasures',
});
const kWarnedEntryTypes = new SafeMap();

const kObservers = new SafeSet();
const kPending = new SafeSet();
let isPending = false;

function queuePending() {
  if (isPending) return;
  isPending = true;
  setImmediate(() => {
    isPending = false;
    const pendings = ArrayFrom(kPending.values());
    kPending.clear();
    for (const pending of pendings)
      pending[kDispatch]();
  });
}

function getObserverType(type) {
  switch (type) {
    case 'gc': return NODE_PERFORMANCE_ENTRY_TYPE_GC;
    case 'http2': return NODE_PERFORMANCE_ENTRY_TYPE_HTTP2;
    case 'http': return NODE_PERFORMANCE_ENTRY_TYPE_HTTP;
    case 'net': return NODE_PERFORMANCE_ENTRY_TYPE_NET;
    case 'dns': return NODE_PERFORMANCE_ENTRY_TYPE_DNS;
    case 'quic': return NODE_PERFORMANCE_ENTRY_TYPE_QUIC;
  }
}

function maybeDecrementObserverCounts(entryTypes) {
  for (const type of entryTypes) {
    const observerType = getObserverType(type);

    if (observerType !== undefined) {
      observerCounts[observerType]--;

      // Removes the GC callbacks once the last 'gc' observer is gone.
      if (observerType === NODE_PERFORMANCE_ENTRY_TYPE_GC) {
        updateGarbageCollectionTracking();
      }
    }
  }
}

let gcTrackingDeserializeCallbackAdded = false;

// V8 GC callbacks do not survive a snapshot. When building one with active
// 'gc' observers, register the callbacks again after deserialization.
function maybeRestoreGarbageCollectionTrackingOnDeserialize() {
  if (gcTrackingDeserializeCallbackAdded) return;
  const {
    namespace: {
      addDeserializeCallback,
      isBuildingSnapshot,
    },
  } = require('internal/v8/startup_snapshot');
  if (!isBuildingSnapshot()) return;
  gcTrackingDeserializeCallbackAdded = true;
  addDeserializeCallback(updateGarbageCollectionTracking);
}

function maybeIncrementObserverCount(type) {
  const observerType = getObserverType(type);

  if (observerType !== undefined) {
    observerCounts[observerType]++;
    // Installs the GC callbacks if they are not installed yet. This is
    // idempotent, so it is called whenever the 'gc' observer count changes.
    if (observerType === NODE_PERFORMANCE_ENTRY_TYPE_GC) {
      updateGarbageCollectionTracking();
      maybeRestoreGarbageCollectionTrackingOnDeserialize();
    }
  }
}

const kSkipThrow = Symbol('kSkipThrow');
const performanceObserverSorter = (first, second) => {
  return first.startTime - second.startTime;
};

class PerformanceObserverEntryList {
  constructor(skipThrowSymbol = undefined, entries = []) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    this[kBuffer] = ArrayPrototypeSort(entries, performanceObserverSorter);
  }

  getEntries() {
    validateThisInternalField(this, kBuffer, 'PerformanceObserverEntryList');
    return ArrayPrototypeSlice(this[kBuffer]);
  }

  getEntriesByType(type) {
    validateThisInternalField(this, kBuffer, 'PerformanceObserverEntryList');
    if (arguments.length === 0) {
      throw new ERR_MISSING_ARGS('type');
    }
    type = `${type}`;
    return ArrayPrototypeFilter(
      this[kBuffer],
      (entry) => entry.entryType === type);
  }

  getEntriesByName(name, type = undefined) {
    validateThisInternalField(this, kBuffer, 'PerformanceObserverEntryList');
    if (arguments.length === 0) {
      throw new ERR_MISSING_ARGS('name');
    }
    name = `${name}`;
    if (type != null /** not nullish */) {
      return ArrayPrototypeFilter(
        this[kBuffer],
        (entry) => entry.name === name && entry.entryType === type);
    }
    return ArrayPrototypeFilter(
      this[kBuffer],
      (entry) => entry.name === name);
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `PerformanceObserverEntryList ${inspect(this[kBuffer], opts)}`;
  }
}
ObjectDefineProperties(PerformanceObserverEntryList.prototype, {
  getEntries: kEnumerableProperty,
  getEntriesByType: kEnumerableProperty,
  getEntriesByName: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    writable: false,
    enumerable: false,
    configurable: true,
    value: 'PerformanceObserverEntryList',
  },
});

class PerformanceObserver {
  #buffer = [];
  #entryTypes = new SafeSet();
  #type;
  #callback;

  constructor(callback) {
    validateFunction(callback, 'callback');
    this.#callback = callback;
  }

  observe(options = kEmptyObject) {
    validateObject(options, 'options');
    const {
      entryTypes,
      type,
      buffered,
    } = { ...options };
    if (entryTypes === undefined && type === undefined)
      throw new ERR_MISSING_ARGS('options.entryTypes', 'options.type');
    if (entryTypes != null && type != null)
      throw new ERR_INVALID_ARG_VALUE('options.entryTypes',
                                      entryTypes,
                                      'options.entryTypes can not set with ' +
                                      'options.type together');

    switch (this.#type) {
      case undefined:
        if (entryTypes !== undefined) this.#type = kTypeMultiple;
        if (type !== undefined) this.#type = kTypeSingle;
        break;
      case kTypeSingle:
        if (entryTypes !== undefined)
          throw lazyDOMException(
            'PerformanceObserver can not change to multiple observations',
            'InvalidModificationError');
        break;
      case kTypeMultiple:
        if (type !== undefined)
          throw lazyDOMException(
            'PerformanceObserver can not change to single observation',
            'InvalidModificationError');
        break;
    }

    if (this.#type === kTypeMultiple) {
      if (!ArrayIsArray(entryTypes)) {
        throw new ERR_INVALID_ARG_TYPE(
          'options.entryTypes',
          'string[]',
          entryTypes);
      }
      maybeDecrementObserverCounts(this.#entryTypes);
      this.#entryTypes.clear();
      for (let n = 0; n < entryTypes.length; n++) {
        const entryType = entryTypes[n];
        // Count each entry type at most once per observer, as disconnect()
        // decrements the counts once per observed type.
        if (ArrayPrototypeIncludes(kSupportedEntryTypes, entryType) &&
            !this.#entryTypes.has(entryType)) {
          this.#entryTypes.add(entryType);
          maybeIncrementObserverCount(entryType);
        }
      }
    } else {
      if (!ArrayPrototypeIncludes(kSupportedEntryTypes, type))
        return;
      // Observing the same type again only replaces the options.
      if (!this.#entryTypes.has(type)) {
        this.#entryTypes.add(type);
        maybeIncrementObserverCount(type);
      }
      if (buffered) {
        const entries = filterBufferMapByNameAndType(undefined, type);
        SafeArrayPrototypePushApply(this.#buffer, entries);
        kPending.add(this);
        if (kPending.size)
          queuePending();
      }
    }

    if (this.#entryTypes.size)
      kObservers.add(this);
    else
      this.disconnect();
  }

  disconnect() {
    maybeDecrementObserverCounts(this.#entryTypes);
    kObservers.delete(this);
    kPending.delete(this);
    this.#buffer = [];
    this.#entryTypes.clear();
    this.#type = undefined;
  }

  takeRecords() {
    const list = this.#buffer;
    this.#buffer = [];
    return list;
  }

  static get supportedEntryTypes() {
    return kSupportedEntryTypes;
  }

  [kMaybeBuffer](entry) {
    if (!this.#entryTypes.has(entry.entryType))
      return;
    ArrayPrototypePush(this.#buffer, entry);
    kPending.add(this);
    if (kPending.size)
      queuePending();
  }

  [kDispatch]() {
    const entryList = new PerformanceObserverEntryList(kSkipThrow, this.takeRecords());

    this.#callback(entryList, this);
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `PerformanceObserver ${inspect({
      connected: kObservers.has(this),
      pending: kPending.has(this),
      entryTypes: ArrayFrom(this.#entryTypes),
      buffer: this.#buffer,
    }, opts)}`;
  }
}
ObjectDefineProperties(PerformanceObserver.prototype, {
  observe: kEnumerableProperty,
  disconnect: kEnumerableProperty,
  takeRecords: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    writable: false,
    enumerable: false,
    configurable: true,
    value: 'PerformanceObserver',
  },
});

/**
 * https://www.w3.org/TR/performance-timeline/#dfn-queue-a-performanceentry
 *
 * Add the performance entry to the interested performance observer's queue.
 */
function enqueue(entry) {
  if (!isPerformanceEntry(entry))
    throw new ERR_INVALID_ARG_TYPE('entry', 'PerformanceEntry', entry);

  for (const obs of kObservers) {
    obs[kMaybeBuffer](entry);
  }
}

/**
 * Add the user timing entry to the global buffer.
 */
function bufferUserTiming(entry) {
  const entryType = entry.entryType;
  let buffer;
  if (entryType === 'mark') {
    buffer = markEntryBuffer;
  } else if (entryType === 'measure') {
    buffer = measureEntryBuffer;
  } else {
    return;
  }

  const count = buffer.push(entry);

  if (count > kPerformanceEntryBufferWarnSize &&
    !kWarnedEntryTypes.has(entryType)) {
    kWarnedEntryTypes.set(entryType, true);
    // No error code for this since it is a Warning
    // eslint-disable-next-line no-restricted-syntax
    const w = new Error('Possible perf_hooks memory leak detected. ' +
                        `${count} ${entryType} entries added to the global ` +
                        'performance entry buffer. Use ' +
                        `${kClearPerformanceEntryBuffers[entryType]} to ` +
                        'clear the buffer.');
    w.name = 'MaxPerformanceEntryBufferExceededWarning';
    w.entryType = entryType;
    w.count = count;
    process.emitWarning(w);
  }
}

/**
 * Add the resource timing entry to the global buffer if the buffer size is not
 * exceeding the buffer limit, or dispatch a buffer full event on the global
 * performance object.
 *
 * See also https://www.w3.org/TR/resource-timing-2/#dfn-add-a-performanceresourcetiming-entry
 */
function bufferResourceTiming(entry) {
  if (resourceTimingBuffer.length < resourceTimingBufferSizeLimit && !resourceTimingBufferFullPending) {
    ArrayPrototypePush(resourceTimingBuffer, entry);
    return;
  }

  if (!resourceTimingBufferFullPending) {
    resourceTimingBufferFullPending = true;
    setImmediate(() => {
      while (resourceTimingSecondaryBuffer.length > 0) {
        const excessNumberBefore = resourceTimingSecondaryBuffer.length;
        dispatchBufferFull('resourcetimingbufferfull');

        // Calculate the number of items to be pushed to the global buffer.
        const numbersToPreserve = MathMax(
          MathMin(resourceTimingBufferSizeLimit - resourceTimingBuffer.length, resourceTimingSecondaryBuffer.length),
          0,
        );
        const excessNumberAfter = resourceTimingSecondaryBuffer.length - numbersToPreserve;
        for (let idx = 0; idx < numbersToPreserve; idx++) {
          ArrayPrototypePush(resourceTimingBuffer, resourceTimingSecondaryBuffer[idx]);
        }

        if (excessNumberBefore <= excessNumberAfter) {
          resourceTimingSecondaryBuffer = [];
        }
      }
      resourceTimingBufferFullPending = false;
    });
  }

  ArrayPrototypePush(resourceTimingSecondaryBuffer, entry);
}

// https://w3c.github.io/resource-timing/#dom-performance-setresourcetimingbuffersize
function setResourceTimingBufferSize(maxSize) {
  // If the maxSize parameter is less than resource timing buffer current
  // size, no PerformanceResourceTiming objects are to be removed from the
  // performance entry buffer.
  resourceTimingBufferSizeLimit = maxSize;
}

function setDispatchBufferFull(fn) {
  dispatchBufferFull = fn;
}

function clearEntriesFromBuffer(type, name) {
  if (type === 'mark') {
    markEntryBuffer.clear(name);
  } else if (type === 'measure') {
    measureEntryBuffer.clear(name);
  } else if (type === 'resource') {
    resourceTimingBuffer = name === undefined ?
      [] : ArrayPrototypeFilter(resourceTimingBuffer, (entry) => entry.name !== name);
  }
}

function filterBufferMapByNameAndType(name, type) {
  if (type === 'mark') {
    return name !== undefined ?
      markEntryBuffer.getByName(name) :
      markEntryBuffer.getAllSorted();
  }
  if (type === 'measure') {
    return name !== undefined ?
      measureEntryBuffer.getByName(name) :
      measureEntryBuffer.getAllSorted();
  }
  if (type === 'resource') {
    const out = name !== undefined ?
      ArrayPrototypeFilter(resourceTimingBuffer, (entry) => entry.name === name) :
      ArrayPrototypeSlice(resourceTimingBuffer);
    return out.length > 1 ? ArrayPrototypeSort(out, performanceObserverSorter) : out;
  }
  if (type !== undefined) {
    // Unrecognized type;
    return [];
  }

  const out = [];
  if (name !== undefined) {
    markEntryBuffer.appendByNameTo(name, out);
    measureEntryBuffer.appendByNameTo(name, out);
    for (let i = 0; i < resourceTimingBuffer.length; i++) {
      if (resourceTimingBuffer[i].name === name) {
        out[out.length] = resourceTimingBuffer[i];
      }
    }
  } else {
    markEntryBuffer.appendAllTo(out);
    measureEntryBuffer.appendAllTo(out);
    for (let i = 0; i < resourceTimingBuffer.length; i++) {
      out[out.length] = resourceTimingBuffer[i];
    }
  }

  return out.length > 1 ? ArrayPrototypeSort(out, performanceObserverSorter) : out;
}

function observerCallback(name, type, startTime, duration, details) {
  const entry =
    createPerformanceNodeEntry(
      name,
      type,
      startTime,
      duration,
      details);

  enqueue(entry);
}

setupObservers(observerCallback);

function hasObserver(type) {
  const observerType = getObserverType(type);
  return observerCounts[observerType] > 0;
}


function startPerf(target, key, context = kEmptyObject) {
  target[key] = {
    ...context,
    startTime: now(),
  };
}

function stopPerf(target, key, context = kEmptyObject) {
  const ctx = target[key];
  if (!ctx) {
    return;
  }
  const startTime = ctx.startTime;
  const entry = createPerformanceNodeEntry(
    ctx.name,
    ctx.type,
    startTime,
    now() - startTime,
    { ...ctx.detail, ...context.detail },
  );
  enqueue(entry);
}

module.exports = {
  PerformanceObserver,
  PerformanceObserverEntryList,
  enqueue,
  hasObserver,
  clearEntriesFromBuffer,
  filterBufferMapByNameAndType,
  startPerf,
  stopPerf,

  bufferUserTiming,
  bufferResourceTiming,
  setResourceTimingBufferSize,
  setDispatchBufferFull,
};
