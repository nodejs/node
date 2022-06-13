'use strict';

const {
  ObjectDefineProperty,
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
} = primordials;

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_MISSING_ARGS
  }
} = require('internal/errors');

const {
  EventTarget,
} = require('internal/event_target');

const { now } = require('internal/perf/utils');

const { markResourceTiming } = require('internal/perf/resource_timing');

const {
  mark,
  measure,
  clearMarkTimings,
} = require('internal/perf/usertiming');
const {
  clearEntriesFromBuffer,
  filterBufferMapByNameAndType,
} = require('internal/perf/observe');

const eventLoopUtilization = require('internal/perf/event_loop_utilization');
const nodeTiming = require('internal/perf/nodetiming');
const timerify = require('internal/perf/timerify');
const { customInspectSymbol: kInspect } = require('internal/util');
const { inspect } = require('util');

const {
  getTimeOriginTimestamp
} = internalBinding('performance');

class Performance extends EventTarget {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `Performance ${inspect({
      nodeTiming: this.nodeTiming,
      timeOrigin: this.timeOrigin,
    }, opts)}`;
  }
}

function toJSON() {
  return {
    nodeTiming: this.nodeTiming,
    timeOrigin: this.timeOrigin,
    eventLoopUtilization: this.eventLoopUtilization()
  };
}

function clearMarks(name) {
  if (name !== undefined) {
    name = `${name}`;
  }
  clearMarkTimings(name);
  clearEntriesFromBuffer('mark', name);
}

function clearMeasures(name) {
  if (name !== undefined) {
    name = `${name}`;
  }
  clearEntriesFromBuffer('measure', name);
}

function clearResourceTimings(name) {
  if (name !== undefined) {
    name = `${name}`;
  }
  clearEntriesFromBuffer('resource', name);
}

function getEntries() {
  return filterBufferMapByNameAndType();
}

function getEntriesByName(name) {
  if (arguments.length === 0) {
    throw new ERR_MISSING_ARGS('name');
  }
  name = `${name}`;
  return filterBufferMapByNameAndType(name, undefined);
}

function getEntriesByType(type) {
  if (arguments.length === 0) {
    throw new ERR_MISSING_ARGS('type');
  }
  type = `${type}`;
  return filterBufferMapByNameAndType(undefined, type);
}

class InternalPerformance extends EventTarget {}
InternalPerformance.prototype.constructor = Performance.prototype.constructor;
ObjectSetPrototypeOf(InternalPerformance.prototype, Performance.prototype);

ObjectDefineProperties(Performance.prototype, {
  clearMarks: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: clearMarks,
  },
  clearMeasures: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: clearMeasures,
  },
  clearResourceTimings: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: clearResourceTimings,
  },
  eventLoopUtilization: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: eventLoopUtilization,
  },
  getEntries: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: getEntries,
  },
  getEntriesByName: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: getEntriesByName,
  },
  getEntriesByType: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: getEntriesByType,
  },
  mark: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: mark,
  },
  measure: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: measure,
  },
  nodeTiming: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: nodeTiming,
  },
  // In the browser, this function is not public.  However, it must be used inside fetch
  // which is a Node.js dependency, not a internal module
  markResourceTiming: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: markResourceTiming,
  },
  now: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: now,
  },
  timerify: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: timerify,
  },
  // This would be updated during pre-execution in case
  // the process is launched from a snapshot.
  // TODO(joyeecheung): we may want to warn about access to
  // this during snapshot building.
  timeOrigin: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: getTimeOriginTimestamp(),
  },
  toJSON: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: toJSON,
  }
});

function refreshTimeOrigin() {
  ObjectDefineProperty(Performance.prototype, 'timeOrigin', {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value: getTimeOriginTimestamp(),
  });
}

module.exports = {
  InternalPerformance,
  refreshTimeOrigin
};
