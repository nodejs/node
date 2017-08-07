'use strict';

const {
  PerformanceEntry,
  mark: _mark,
  measure: _measure,
  milestones,
  observerCounts,
  setupObservers,
  timeOrigin,
  timerify,
  constants
} = process.binding('performance');

const {
  NODE_PERFORMANCE_ENTRY_TYPE_NODE,
  NODE_PERFORMANCE_ENTRY_TYPE_MARK,
  NODE_PERFORMANCE_ENTRY_TYPE_MEASURE,
  NODE_PERFORMANCE_ENTRY_TYPE_GC,
  NODE_PERFORMANCE_ENTRY_TYPE_FUNCTION,

  NODE_PERFORMANCE_MILESTONE_NODE_START,
  NODE_PERFORMANCE_MILESTONE_V8_START,
  NODE_PERFORMANCE_MILESTONE_LOOP_START,
  NODE_PERFORMANCE_MILESTONE_LOOP_EXIT,
  NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE,
  NODE_PERFORMANCE_MILESTONE_ENVIRONMENT,
  NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_START,
  NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_END,
  NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_START,
  NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_END,
  NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START,
  NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END,
  NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START,
  NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END
} = constants;

const L = require('internal/linkedlist');
const kInspect = require('internal/util').customInspectSymbol;
const { inherits } = require('util');

const kCallback = Symbol('callback');
const kTypes = Symbol('types');
const kEntries = Symbol('entries');
const kBuffer = Symbol('buffer');
const kBuffering = Symbol('buffering');
const kQueued = Symbol('queued');
const kTimerified = Symbol('timerified');
const kInsertEntry = Symbol('insert-entry');
const kIndexEntry = Symbol('index-entry');
const kClearEntry = Symbol('clear-entry');
const kGetEntries = Symbol('get-entries');
const kIndex = Symbol('index');
const kMarks = Symbol('marks');

observerCounts[NODE_PERFORMANCE_ENTRY_TYPE_MARK] = 1;
observerCounts[NODE_PERFORMANCE_ENTRY_TYPE_MEASURE] = 1;
const observers = {};
const observerableTypes = [
  'node',
  'mark',
  'measure',
  'gc',
  'function'
];

let errors;
function lazyErrors() {
  if (errors === undefined)
    errors = require('internal/errors');
  return errors;
}

function now() {
  const hr = process.hrtime();
  return hr[0] * 1000 + hr[1] / 1e6;
}

class PerformanceNodeTiming {
  constructor() {}

  get name() {
    return 'node';
  }

  get entryType() {
    return 'node';
  }

  get startTime() {
    return timeOrigin;
  }

  get duration() {
    return now() - timeOrigin;
  }

  get nodeStart() {
    return milestones[NODE_PERFORMANCE_MILESTONE_NODE_START];
  }

  get v8Start() {
    return milestones[NODE_PERFORMANCE_MILESTONE_V8_START];
  }

  get environment() {
    return milestones[NODE_PERFORMANCE_MILESTONE_ENVIRONMENT];
  }

  get loopStart() {
    return milestones[NODE_PERFORMANCE_MILESTONE_LOOP_START];
  }

  get loopExit() {
    return milestones[NODE_PERFORMANCE_MILESTONE_LOOP_EXIT];
  }

  get bootstrapComplete() {
    return milestones[NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE];
  }

  get thirdPartyMainStart() {
    return milestones[NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_START];
  }

  get thirdPartyMainEnd() {
    return milestones[NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_END];
  }

  get clusterSetupStart() {
    return milestones[NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_START];
  }

  get clusterSetupEnd() {
    return milestones[NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_END];
  }

  get moduleLoadStart() {
    return milestones[NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START];
  }

  get moduleLoadEnd() {
    return milestones[NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END];
  }

  get preloadModuleLoadStart() {
    return milestones[NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START];
  }

  get preloadModuleLoadEnd() {
    return milestones[NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END];
  }

  [kInspect]() {
    return {
      name: 'node',
      entryType: 'node',
      startTime: this.startTime,
      duration: this.duration,
      nodeStart: this.nodeStart,
      v8Start: this.v8Start,
      bootstrapComplete: this.bootstrapComplete,
      environment: this.environment,
      loopStart: this.loopStart,
      loopExit: this.loopExit,
      thirdPartyMainStart: this.thirdPartyMainStart,
      thirdPartyMainEnd: this.thirdPartyMainEnd,
      clusterSetupStart: this.clusterSetupStart,
      clusterSetupEnd: this.clusterSetupEnd,
      moduleLoadStart: this.moduleLoadStart,
      moduleLoadEnd: this.moduleLoadEnd,
      preloadModuleLoadStart: this.preloadModuleLoadStart,
      preloadModuleLoadEnd: this.preloadModuleLoadEnd
    };
  }
}
// Use this instead of Extends because we want PerformanceEntry in the
// prototype chain but we do not want to use the PerformanceEntry
// constructor for this.
inherits(PerformanceNodeTiming, PerformanceEntry);

const nodeTiming = new PerformanceNodeTiming();

// Maintains a list of entries as a linked list stored in insertion order.
class PerformanceObserverEntryList {
  constructor() {
    Object.defineProperty(this, kEntries, {
      writable: true,
      enumerable: false,
      value: {}
    });
    L.init(this[kEntries]);
  }

  [kInsertEntry](entry) {
    const item = { entry };
    L.append(this[kEntries], item);
    this[kIndexEntry](item);
  }

  [kIndexEntry](entry) {
    // Default implementation does nothing
  }

  [kGetEntries](name, type) {
    const ret = [];
    const list = this[kEntries];
    if (!L.isEmpty(list)) {
      let item = L.peek(list);
      while (item && item !== list) {
        const entry = item.entry;
        if ((name && entry.name !== name) ||
            (type && entry.entryType !== type)) {
          item = item._idlePrev;
          continue;
        }
        sortedInsert(ret, entry);
        item = item._idlePrev;
      }
    }
    return ret;
  }

  // While the items are stored in insertion order, getEntries() is
  // required to return items sorted by startTime.
  getEntries() {
    return this[kGetEntries]();
  }

  getEntriesByType(type) {
    return this[kGetEntries](undefined, `${type}`);
  }

  getEntriesByName(name, type) {
    return this[kGetEntries](`${name}`, type !== undefined ? `${type}` : type);
  }
}

class PerformanceObserver {
  constructor(callback) {
    if (typeof callback !== 'function') {
      const errors = lazyErrors();
      throw new errors.TypeError('ERR_INVALID_CALLBACK');
    }
    Object.defineProperties(this, {
      [kTypes]: {
        enumerable: false,
        writable: true,
        value: {}
      },
      [kCallback]: {
        enumerable: false,
        writable: true,
        value: callback
      },
      [kBuffer]: {
        enumerable: false,
        writable: true,
        value: new PerformanceObserverEntryList()
      },
      [kBuffering]: {
        enumerable: false,
        writable: true,
        value: false
      },
      [kQueued]: {
        enumerable: false,
        writable: true,
        value: false
      }
    });
  }

  disconnect() {
    const types = this[kTypes];
    const keys = Object.keys(types);
    for (var n = 0; n < keys.length; n++) {
      const item = types[keys[n]];
      if (item) {
        L.remove(item);
        observerCounts[keys[n]]--;
      }
    }
    this[kTypes] = {};
  }

  observe(options) {
    const errors = lazyErrors();
    if (typeof options !== 'object' || options == null) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options', 'Object');
    }
    if (!Array.isArray(options.entryTypes)) {
      throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                 'entryTypes', options);
    }
    const entryTypes = options.entryTypes.filter(filterTypes).map(mapTypes);
    if (entryTypes.length === 0) {
      throw new errors.Error('ERR_VALID_PERFORMANCE_ENTRY_TYPE');
    }
    this.disconnect();
    this[kBuffer][kEntries] = [];
    L.init(this[kBuffer][kEntries]);
    this[kBuffering] = Boolean(options.buffered);
    for (var n = 0; n < entryTypes.length; n++) {
      const entryType = entryTypes[n];
      const list = getObserversList(entryType);
      const item = { obs: this };
      this[kTypes][entryType] = item;
      L.append(list, item);
      observerCounts[entryType]++;
    }
  }
}

class Performance extends PerformanceObserverEntryList {
  constructor() {
    super();
    this[kIndex] = {
      [kMarks]: new Set()
    };
    this[kInsertEntry](nodeTiming);
  }

  [kIndexEntry](item) {
    const index = this[kIndex];
    const type = item.entry.entryType;
    let items = index[type];
    if (!items) {
      items = index[type] = {};
      L.init(items);
    }
    const entry = item.entry;
    L.append(items, { entry, item });
  }

  [kClearEntry](type, name) {
    const index = this[kIndex];
    const items = index[type];
    if (!items) return;
    let item = L.peek(items);
    while (item && item !== items) {
      const entry = item.entry;
      const next = item._idlePrev;
      if (name !== undefined) {
        if (entry.name === `${name}`) {
          L.remove(item); // remove from the index
          L.remove(item.item); // remove from the master
        }
      } else {
        L.remove(item); // remove from the index
        L.remove(item.item); // remove from the master
      }
      item = next;
    }
  }

  get nodeTiming() {
    return nodeTiming;
  }

  get timeOrigin() {
    return timeOrigin;
  }

  now() {
    return now();
  }

  mark(name) {
    name = `${name}`;
    _mark(name);
    this[kIndex][kMarks].add(name);
  }

  measure(name, startMark, endMark) {
    name = `${name}`;
    endMark = `${endMark}`;
    startMark = startMark !== undefined ? `${startMark}` : '';
    const marks = this[kIndex][kMarks];
    if (!marks.has(endMark) && !(endMark in nodeTiming)) {
      const errors = lazyErrors();
      throw new errors.Error('ERR_INVALID_PERFORMANCE_MARK', endMark);
    }
    _measure(name, startMark, endMark);
  }

  clearMarks(name) {
    name = name !== undefined ? `${name}` : name;
    this[kClearEntry]('mark', name);
    if (name !== undefined)
      this[kIndex][kMarks].delete(name);
    else
      this[kIndex][kMarks].clear();
  }

  clearMeasures(name) {
    this[kClearEntry]('measure', name);
  }

  clearGC() {
    this[kClearEntry]('gc');
  }

  clearFunctions(name) {
    this[kClearEntry]('function', name);
  }

  timerify(fn) {
    if (typeof fn !== 'function') {
      const errors = lazyErrors();
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'fn', 'Function');
    }
    if (fn[kTimerified])
      return fn[kTimerified];
    const ret = timerify(fn, fn.length);
    Object.defineProperty(fn, kTimerified, {
      enumerable: false,
      configurable: true,
      writable: false,
      value: ret
    });
    Object.defineProperties(ret, {
      [kTimerified]: {
        enumerable: false,
        configurable: true,
        writable: false,
        value: ret
      },
      name: {
        enumerable: false,
        configurable: true,
        writable: false,
        value: `timerified ${fn.name}`
      }
    });
    return ret;
  }

  [kInspect]() {
    return {
      timeOrigin,
      nodeTiming
    };
  }
}

const performance = new Performance();

function getObserversList(type) {
  let list = observers[type];
  if (list === undefined) {
    list = observers[type] = {};
    L.init(list);
  }
  return list;
}

function doNotify() {
  this[kQueued] = false;
  this[kCallback](this[kBuffer], this);
  this[kBuffer][kEntries] = [];
  L.init(this[kBuffer][kEntries]);
}

// Set up the callback used to receive PerformanceObserver notifications
function observersCallback(entry) {
  const type = mapTypes(entry.entryType);
  performance[kInsertEntry](entry);
  const list = getObserversList(type);

  let current = L.peek(list);

  while (current && current.obs) {
    const observer = current.obs;
    // First, add the item to the observers buffer
    const buffer = observer[kBuffer];
    buffer[kInsertEntry](entry);
    // Second, check to see if we're buffering
    if (observer[kBuffering]) {
      // If we are, schedule a setImmediate call if one hasn't already
      if (!observer[kQueued]) {
        observer[kQueued] = true;
        // Use setImmediate instead of nextTick to give more time
        // for multiple entries to collect.
        setImmediate(doNotify.bind(observer));
      }
    } else {
      // If not buffering, notify immediately
      doNotify.call(observer);
    }
    current = current._idlePrev;
  }
}
setupObservers(observersCallback);

function filterTypes(i) {
  return observerableTypes.indexOf(`${i}`) >= 0;
}

function mapTypes(i) {
  switch (i) {
    case 'node': return NODE_PERFORMANCE_ENTRY_TYPE_NODE;
    case 'mark': return NODE_PERFORMANCE_ENTRY_TYPE_MARK;
    case 'measure': return NODE_PERFORMANCE_ENTRY_TYPE_MEASURE;
    case 'gc': return NODE_PERFORMANCE_ENTRY_TYPE_GC;
    case 'function': return NODE_PERFORMANCE_ENTRY_TYPE_FUNCTION;
  }
}

// The specification requires that PerformanceEntry instances are sorted
// according to startTime. Unfortunately, they are not necessarily created
// in that same order, and can be reported to the JS layer in any order,
// which means we need to keep the list sorted as we insert.
function getInsertLocation(list, entryStartTime) {
  let start = 0;
  let end = list.length;
  while (start < end) {
    const pivot = (end + start) >>> 1;
    if (list[pivot].startTime === entryStartTime)
      return pivot;
    if (list[pivot].startTime < entryStartTime)
      start = pivot + 1;
    else
      end = pivot;
  }
  return start;
}

function sortedInsert(list, entry) {
  const entryStartTime = entry.startTime;
  if (list.length === 0 ||
      (list[list.length - 1].startTime < entryStartTime)) {
    list.push(entry);
    return;
  }
  if (list[0] && (list[0].startTime > entryStartTime)) {
    list.unshift(entry);
    return;
  }
  const location = getInsertLocation(list, entryStartTime);
  list.splice(location, 0, entry);
}

module.exports = {
  performance,
  PerformanceObserver
};

Object.defineProperty(module.exports, 'constants', {
  configurable: false,
  enumerable: true,
  value: constants
});
