'use strict';

const {
  ArrayIsArray,
  Boolean,
  NumberIsSafeInteger,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectKeys,
  Set,
  Symbol,
} = primordials;

const {
  ELDHistogram: _ELDHistogram,
  PerformanceEntry,
  mark: _mark,
  clearMark: _clearMark,
  measure: _measure,
  milestones,
  observerCounts,
  setupObservers,
  timeOrigin,
  timeOriginTimestamp,
  timerify,
  constants,
  installGarbageCollectionTracking,
  removeGarbageCollectionTracking
} = internalBinding('performance');

const {
  NODE_PERFORMANCE_ENTRY_TYPE_NODE,
  NODE_PERFORMANCE_ENTRY_TYPE_MARK,
  NODE_PERFORMANCE_ENTRY_TYPE_MEASURE,
  NODE_PERFORMANCE_ENTRY_TYPE_GC,
  NODE_PERFORMANCE_ENTRY_TYPE_FUNCTION,
  NODE_PERFORMANCE_ENTRY_TYPE_HTTP2,
  NODE_PERFORMANCE_ENTRY_TYPE_HTTP,

  NODE_PERFORMANCE_MILESTONE_NODE_START,
  NODE_PERFORMANCE_MILESTONE_V8_START,
  NODE_PERFORMANCE_MILESTONE_LOOP_START,
  NODE_PERFORMANCE_MILESTONE_LOOP_EXIT,
  NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE,
  NODE_PERFORMANCE_MILESTONE_ENVIRONMENT
} = constants;

const { AsyncResource } = require('async_hooks');
const L = require('internal/linkedlist');
const kInspect = require('internal/util').customInspectSymbol;

const {
  ERR_INVALID_CALLBACK,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_OPT_VALUE,
  ERR_VALID_PERFORMANCE_ENTRY_TYPE,
  ERR_INVALID_PERFORMANCE_MARK
} = require('internal/errors').codes;

const {
  Histogram,
  kHandle,
} = require('internal/histogram');

const { setImmediate } = require('timers');
const kCallback = Symbol('callback');
const kTypes = Symbol('types');
const kEntries = Symbol('entries');
const kBuffer = Symbol('buffer');
const kBuffering = Symbol('buffering');
const kQueued = Symbol('queued');
const kTimerified = Symbol('timerified');
const kInsertEntry = Symbol('insert-entry');
const kGetEntries = Symbol('get-entries');
const kIndex = Symbol('index');
const kMarks = Symbol('marks');
const kCount = Symbol('count');

const observers = {};
const observerableTypes = [
  'node',
  'mark',
  'measure',
  'gc',
  'function',
  'http2',
  'http'
];

const IDX_STREAM_STATS_ID = 0;
const IDX_STREAM_STATS_TIMETOFIRSTBYTE = 1;
const IDX_STREAM_STATS_TIMETOFIRSTHEADER = 2;
const IDX_STREAM_STATS_TIMETOFIRSTBYTESENT = 3;
const IDX_STREAM_STATS_SENTBYTES = 4;
const IDX_STREAM_STATS_RECEIVEDBYTES = 5;

const IDX_SESSION_STATS_TYPE = 0;
const IDX_SESSION_STATS_PINGRTT = 1;
const IDX_SESSION_STATS_FRAMESRECEIVED = 2;
const IDX_SESSION_STATS_FRAMESSENT = 3;
const IDX_SESSION_STATS_STREAMCOUNT = 4;
const IDX_SESSION_STATS_STREAMAVERAGEDURATION = 5;
const IDX_SESSION_STATS_DATA_SENT = 6;
const IDX_SESSION_STATS_DATA_RECEIVED = 7;
const IDX_SESSION_STATS_MAX_CONCURRENT_STREAMS = 8;

let http2;
let sessionStats;
let streamStats;

function collectHttp2Stats(entry) {
  if (http2 === undefined) http2 = internalBinding('http2');
  switch (entry.name) {
    case 'Http2Stream':
      if (streamStats === undefined)
        streamStats = http2.streamStats;
      entry.id =
        streamStats[IDX_STREAM_STATS_ID] >>> 0;
      entry.timeToFirstByte =
        streamStats[IDX_STREAM_STATS_TIMETOFIRSTBYTE];
      entry.timeToFirstHeader =
        streamStats[IDX_STREAM_STATS_TIMETOFIRSTHEADER];
      entry.timeToFirstByteSent =
        streamStats[IDX_STREAM_STATS_TIMETOFIRSTBYTESENT];
      entry.bytesWritten =
        streamStats[IDX_STREAM_STATS_SENTBYTES];
      entry.bytesRead =
        streamStats[IDX_STREAM_STATS_RECEIVEDBYTES];
      break;
    case 'Http2Session':
      if (sessionStats === undefined)
        sessionStats = http2.sessionStats;
      entry.type =
        sessionStats[IDX_SESSION_STATS_TYPE] >>> 0 === 0 ? 'server' : 'client';
      entry.pingRTT =
        sessionStats[IDX_SESSION_STATS_PINGRTT];
      entry.framesReceived =
        sessionStats[IDX_SESSION_STATS_FRAMESRECEIVED];
      entry.framesSent =
        sessionStats[IDX_SESSION_STATS_FRAMESSENT];
      entry.streamCount =
        sessionStats[IDX_SESSION_STATS_STREAMCOUNT];
      entry.streamAverageDuration =
        sessionStats[IDX_SESSION_STATS_STREAMAVERAGEDURATION];
      entry.bytesWritten =
        sessionStats[IDX_SESSION_STATS_DATA_SENT];
      entry.bytesRead =
        sessionStats[IDX_SESSION_STATS_DATA_RECEIVED];
      entry.maxConcurrentStreams =
        sessionStats[IDX_SESSION_STATS_MAX_CONCURRENT_STREAMS];
      break;
  }
}

function now() {
  const hr = process.hrtime();
  return hr[0] * 1000 + hr[1] / 1e6;
}

function getMilestoneTimestamp(milestoneIdx) {
  const ns = milestones[milestoneIdx];
  if (ns === -1)
    return ns;
  return ns / 1e6 - timeOrigin;
}

class PerformanceNodeTiming extends PerformanceEntry {
  get name() {
    return 'node';
  }

  get entryType() {
    return 'node';
  }

  get startTime() {
    return 0;
  }

  get duration() {
    return now() - timeOrigin;
  }

  get nodeStart() {
    return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_NODE_START);
  }

  get v8Start() {
    return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_V8_START);
  }

  get environment() {
    return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_ENVIRONMENT);
  }

  get loopStart() {
    return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_LOOP_START);
  }

  get loopExit() {
    return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_LOOP_EXIT);
  }

  get bootstrapComplete() {
    return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE);
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
      loopExit: this.loopExit
    };
  }
}

const nodeTiming = new PerformanceNodeTiming();

// Maintains a list of entries as a linked list stored in insertion order.
class PerformanceObserverEntryList {
  constructor() {
    ObjectDefineProperties(this, {
      [kEntries]: {
        writable: true,
        enumerable: false,
        value: {}
      },
      [kCount]: {
        writable: true,
        enumerable: false,
        value: 0
      }
    });
    L.init(this[kEntries]);
  }

  [kInsertEntry](entry) {
    const item = { entry };
    L.append(this[kEntries], item);
    this[kCount]++;
  }

  get length() {
    return this[kCount];
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

class PerformanceObserver extends AsyncResource {
  constructor(callback) {
    if (typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK(callback);
    }
    super('PerformanceObserver');
    ObjectDefineProperties(this, {
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
    const observerCountsGC = observerCounts[NODE_PERFORMANCE_ENTRY_TYPE_GC];
    const types = this[kTypes];
    for (const key of ObjectKeys(types)) {
      const item = types[key];
      if (item) {
        L.remove(item);
        observerCounts[key]--;
      }
    }
    this[kTypes] = {};
    if (observerCountsGC === 1 &&
      observerCounts[NODE_PERFORMANCE_ENTRY_TYPE_GC] === 0) {
      removeGarbageCollectionTracking();
    }
  }

  observe(options) {
    if (typeof options !== 'object' || options === null) {
      throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
    }
    if (!ArrayIsArray(options.entryTypes)) {
      throw new ERR_INVALID_OPT_VALUE('entryTypes', options);
    }
    const entryTypes = options.entryTypes.filter(filterTypes).map(mapTypes);
    if (entryTypes.length === 0) {
      throw new ERR_VALID_PERFORMANCE_ENTRY_TYPE();
    }
    this.disconnect();
    const observerCountsGC = observerCounts[NODE_PERFORMANCE_ENTRY_TYPE_GC];
    this[kBuffer][kEntries] = [];
    L.init(this[kBuffer][kEntries]);
    this[kBuffering] = Boolean(options.buffered);
    for (const entryType of entryTypes) {
      const list = getObserversList(entryType);
      if (this[kTypes][entryType]) continue;
      const item = { obs: this };
      this[kTypes][entryType] = item;
      L.append(list, item);
      observerCounts[entryType]++;
    }
    if (observerCountsGC === 0 &&
      observerCounts[NODE_PERFORMANCE_ENTRY_TYPE_GC] === 1) {
      installGarbageCollectionTracking();
    }
  }
}

class Performance {
  constructor() {
    this[kIndex] = {
      [kMarks]: new Set()
    };
  }

  get nodeTiming() {
    return nodeTiming;
  }

  get timeOrigin() {
    return timeOriginTimestamp;
  }

  now() {
    return now() - timeOrigin;
  }

  mark(name) {
    name = `${name}`;
    _mark(name);
    this[kIndex][kMarks].add(name);
  }

  measure(name, startMark, endMark) {
    name = `${name}`;
    const marks = this[kIndex][kMarks];
    if (arguments.length >= 3) {
      if (!marks.has(endMark) && !(endMark in nodeTiming))
        throw new ERR_INVALID_PERFORMANCE_MARK(endMark);
      else
        endMark = `${endMark}`;
    }
    startMark = startMark !== undefined ? `${startMark}` : '';
    _measure(name, startMark, endMark);
  }

  clearMarks(name) {
    name = name !== undefined ? `${name}` : name;
    if (name !== undefined) {
      this[kIndex][kMarks].delete(name);
      _clearMark(name);
    } else {
      this[kIndex][kMarks].clear();
      _clearMark();
    }
  }

  timerify(fn) {
    if (typeof fn !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('fn', 'Function', fn);
    }
    if (fn[kTimerified])
      return fn[kTimerified];
    const ret = timerify(fn, fn.length);
    ObjectDefineProperty(fn, kTimerified, {
      enumerable: false,
      configurable: true,
      writable: false,
      value: ret
    });
    ObjectDefineProperties(ret, {
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
      nodeTiming: this.nodeTiming,
      timeOrigin: this.timeOrigin
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

function doNotify(observer) {
  observer[kQueued] = false;
  observer.runInAsyncScope(observer[kCallback],
                           observer,
                           observer[kBuffer],
                           observer);
  observer[kBuffer][kEntries] = [];
  L.init(observer[kBuffer][kEntries]);
}

// Set up the callback used to receive PerformanceObserver notifications
function observersCallback(entry) {
  const type = mapTypes(entry.entryType);

  if (type === NODE_PERFORMANCE_ENTRY_TYPE_HTTP2)
    collectHttp2Stats(entry);

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
        setImmediate(doNotify, observer);
      }
    } else {
      // If not buffering, notify immediately
      doNotify(observer);
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
    case 'http2': return NODE_PERFORMANCE_ENTRY_TYPE_HTTP2;
    case 'http': return NODE_PERFORMANCE_ENTRY_TYPE_HTTP;
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

class ELDHistogram extends Histogram {
  enable() { return this[kHandle].enable(); }
  disable() { return this[kHandle].disable(); }
}

function monitorEventLoopDelay(options = {}) {
  if (typeof options !== 'object' || options === null) {
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  }
  const { resolution = 10 } = options;
  if (typeof resolution !== 'number') {
    throw new ERR_INVALID_ARG_TYPE('options.resolution',
                                   'number', resolution);
  }
  if (resolution <= 0 || !NumberIsSafeInteger(resolution)) {
    throw new ERR_INVALID_OPT_VALUE.RangeError('resolution', resolution);
  }
  return new ELDHistogram(new _ELDHistogram(resolution));
}

module.exports = {
  performance,
  PerformanceObserver,
  monitorEventLoopDelay
};

ObjectDefineProperty(module.exports, 'constants', {
  configurable: false,
  enumerable: true,
  value: constants
});
