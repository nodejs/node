'use strict';

const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  ArrayPrototypePushApply,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  ArrayPrototypeConcat,
  Error,
  ObjectDefineProperties,
  ObjectFreeze,
  ObjectKeys,
  SafeMap,
  SafeSet,
  Symbol,
} = primordials;

const {
  constants: {
    NODE_PERFORMANCE_ENTRY_TYPE_GC,
    NODE_PERFORMANCE_ENTRY_TYPE_HTTP2,
    NODE_PERFORMANCE_ENTRY_TYPE_HTTP,
  },
  installGarbageCollectionTracking,
  observerCounts,
  removeGarbageCollectionTracking,
  setupObservers,
} = internalBinding('performance');

const {
  InternalPerformanceEntry,
  isPerformanceEntry,
} = require('internal/perf/performance_entry');

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_ARG_TYPE,
    ERR_MISSING_ARGS,
  },
} = require('internal/errors');

const {
  validateCallback,
  validateObject,
} = require('internal/validators');

const {
  customInspectSymbol: kInspect,
  deprecate,
  lazyDOMException,
} = require('internal/util');

const {
  setImmediate,
} = require('timers');

const { inspect } = require('util');

const kBuffer = Symbol('kBuffer');
const kCallback = Symbol('kCallback');
const kDispatch = Symbol('kDispatch');
const kEntryTypes = Symbol('kEntryTypes');
const kMaybeBuffer = Symbol('kMaybeBuffer');
const kDeprecatedFields = Symbol('kDeprecatedFields');
const kType = Symbol('kType');

const kDeprecationMessage =
  'Custom PerformanceEntry accessors are deprecated. ' +
  'Please use the detail property.';

const kTypeSingle = 0;
const kTypeMultiple = 1;

let gcTrackingInstalled = false;

const kSupportedEntryTypes = ObjectFreeze([
  'function',
  'gc',
  'http',
  'http2',
  'mark',
  'measure',
]);

// Performance timeline entry Buffers
let markEntryBuffer = [];
let measureEntryBuffer = [];
const kMaxPerformanceEntryBuffers = 1e6;
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
  }
}

function maybeDecrementObserverCounts(entryTypes) {
  for (const type of entryTypes) {
    const observerType = getObserverType(type);

    if (observerType !== undefined) {
      observerCounts[observerType]--;

      if (observerType === NODE_PERFORMANCE_ENTRY_TYPE_GC &&
          observerCounts[observerType] === 0) {
        removeGarbageCollectionTracking();
        gcTrackingInstalled = false;
      }
    }
  }
}

function maybeIncrementObserverCount(type) {
  const observerType = getObserverType(type);

  if (observerType !== undefined) {
    observerCounts[observerType]++;
    if (!gcTrackingInstalled &&
        observerType === NODE_PERFORMANCE_ENTRY_TYPE_GC) {
      installGarbageCollectionTracking();
      gcTrackingInstalled = true;
    }
  }
}

class PerformanceObserverEntryList {
  constructor(entries) {
    this[kBuffer] = ArrayPrototypeSort(entries, (first, second) => {
      return first.startTime - second.startTime;
    });
  }

  getEntries() {
    return ArrayPrototypeSlice(this[kBuffer]);
  }

  getEntriesByType(type) {
    type = `${type}`;
    return ArrayPrototypeFilter(
      this[kBuffer],
      (entry) => entry.entryType === type);
  }

  getEntriesByName(name, type) {
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
      depth: options.depth == null ? null : options.depth - 1
    };

    return `PerformanceObserverEntryList ${inspect(this[kBuffer], opts)}`;
  }
}

class PerformanceObserver {
  constructor(callback) {
    // TODO(joyeecheung): V8 snapshot does not support instance member
    // initializers for now:
    // https://bugs.chromium.org/p/v8/issues/detail?id=10704
    this[kBuffer] = [];
    this[kEntryTypes] = new SafeSet();
    this[kType] = undefined;
    validateCallback(callback);
    this[kCallback] = callback;
  }

  observe(options = {}) {
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

    switch (this[kType]) {
      case undefined:
        if (entryTypes !== undefined) this[kType] = kTypeMultiple;
        if (type !== undefined) this[kType] = kTypeSingle;
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

    if (this[kType] === kTypeMultiple) {
      if (!ArrayIsArray(entryTypes)) {
        throw new ERR_INVALID_ARG_TYPE(
          'options.entryTypes',
          'string[]',
          entryTypes);
      }
      maybeDecrementObserverCounts(this[kEntryTypes]);
      this[kEntryTypes].clear();
      for (let n = 0; n < entryTypes.length; n++) {
        if (ArrayPrototypeIncludes(kSupportedEntryTypes, entryTypes[n])) {
          this[kEntryTypes].add(entryTypes[n]);
          maybeIncrementObserverCount(entryTypes[n]);
        }
      }
    } else {
      if (!ArrayPrototypeIncludes(kSupportedEntryTypes, type))
        return;
      this[kEntryTypes].add(type);
      maybeIncrementObserverCount(type);
      if (buffered) {
        const entries = filterBufferMapByNameAndType(undefined, type);
        ArrayPrototypePushApply(this[kBuffer], entries);
        kPending.add(this);
        if (kPending.size)
          queuePending();
      }
    }

    if (this[kEntryTypes].size)
      kObservers.add(this);
    else
      this.disconnect();
  }

  disconnect() {
    maybeDecrementObserverCounts(this[kEntryTypes]);
    kObservers.delete(this);
    kPending.delete(this);
    this[kBuffer] = [];
    this[kEntryTypes].clear();
    this[kType] = undefined;
  }

  takeRecords() {
    const list = this[kBuffer];
    this[kBuffer] = [];
    return list;
  }

  static get supportedEntryTypes() {
    return kSupportedEntryTypes;
  }

  [kMaybeBuffer](entry) {
    if (!this[kEntryTypes].has(entry.entryType))
      return;
    ArrayPrototypePush(this[kBuffer], entry);
    kPending.add(this);
    if (kPending.size)
      queuePending();
  }

  [kDispatch]() {
    this[kCallback](new PerformanceObserverEntryList(this.takeRecords()),
                    this);
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `PerformanceObserver ${inspect({
      connected: kObservers.has(this),
      pending: kPending.has(this),
      entryTypes: ArrayFrom(this[kEntryTypes]),
      buffer: this[kBuffer],
    }, opts)}`;
  }
}

function enqueue(entry) {
  if (!isPerformanceEntry(entry))
    throw new ERR_INVALID_ARG_TYPE('entry', 'PerformanceEntry', entry);

  for (const obs of kObservers) {
    obs[kMaybeBuffer](entry);
  }

  const entryType = entry.entryType;
  let buffer;
  if (entryType === 'mark') {
    buffer = markEntryBuffer;
  } else if (entryType === 'measure') {
    buffer = measureEntryBuffer;
  } else {
    return;
  }

  ArrayPrototypePush(buffer, entry);
  const count = buffer.length;

  if (count > kMaxPerformanceEntryBuffers &&
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

function clearEntriesFromBuffer(type, name) {
  if (type !== 'mark' && type !== 'measure') {
    return;
  }

  if (type === 'mark') {
    markEntryBuffer = name === undefined ?
      [] : ArrayPrototypeFilter(markEntryBuffer, (entry) => entry.name !== name);
  } else {
    measureEntryBuffer = name === undefined ?
      [] : ArrayPrototypeFilter(measureEntryBuffer, (entry) => entry.name !== name);
  }
}

function filterBufferMapByNameAndType(name, type) {
  let bufferList;
  if (type === 'mark') {
    bufferList = markEntryBuffer;
  } else if (type === 'measure') {
    bufferList = measureEntryBuffer;
  } else if (type !== undefined) {
    // Unrecognized type;
    return [];
  } else {
    bufferList = ArrayPrototypeConcat(markEntryBuffer, measureEntryBuffer);
  }
  if (name !== undefined) {
    bufferList = ArrayPrototypeFilter(bufferList, (buffer) => buffer.name === name);
  } else if (type !== undefined) {
    bufferList = ArrayPrototypeSlice(bufferList);
  }

  return ArrayPrototypeSort(bufferList, (first, second) => {
    return first.startTime - second.startTime;
  });
}

function observerCallback(name, type, startTime, duration, details) {
  const entry =
    new InternalPerformanceEntry(
      name,
      type,
      startTime,
      duration,
      details);

  if (details !== undefined) {
    // GC, HTTP2, and HTTP PerformanceEntry used additional
    // properties directly off the entry. Those have been
    // moved into the details property. The existing accessors
    // are still included but are deprecated.
    entry[kDeprecatedFields] = new SafeMap();

    const detailKeys = ObjectKeys(details);
    const props = {};
    for (let n = 0; n < detailKeys.length; n++) {
      const key = detailKeys[n];
      entry[kDeprecatedFields].set(key, details[key]);
      props[key] = {
        configurable: true,
        enumerable: true,
        get: deprecate(() => {
          return entry[kDeprecatedFields].get(key);
        }, kDeprecationMessage, 'DEP0152'),
        set: deprecate((value) => {
          entry[kDeprecatedFields].set(key, value);
        }, kDeprecationMessage, 'DEP0152'),
      };
    }
    ObjectDefineProperties(entry, props);
  }

  enqueue(entry);
}

setupObservers(observerCallback);

function hasObserver(type) {
  const observerType = getObserverType(type);
  return observerCounts[observerType] > 0;
}

module.exports = {
  PerformanceObserver,
  PerformanceObserverEntryList,
  enqueue,
  hasObserver,
  clearEntriesFromBuffer,
  filterBufferMapByNameAndType,
};
