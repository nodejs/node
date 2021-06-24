'use strict';

const {
  ObjectDefineProperty,
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
  TypeError,
} = primordials;

const {
  EventTarget,
} = require('internal/event_target');

const { now } = require('internal/perf/utils');

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
    // eslint-disable-next-line no-restricted-syntax
    throw new TypeError('Illegal constructor');
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

function getEntries() {
  return filterBufferMapByNameAndType();
}

function getEntriesByName(name) {
  if (name !== undefined) {
    name = `${name}`;
  }
  return filterBufferMapByNameAndType(name, undefined);
}

function getEntriesByType(type) {
  if (type !== undefined) {
    type = `${type}`;
  }
  return filterBufferMapByNameAndType(undefined, type);
}

class InternalPerformance extends EventTarget {}
InternalPerformance.prototype.constructor = Performance.prototype.constructor;
ObjectSetPrototypeOf(InternalPerformance.prototype, Performance.prototype);

ObjectDefineProperties(Performance.prototype, {
  clearMarks: {
    configurable: true,
    enumerable: false,
    value: clearMarks,
  },
  clearMeasures: {
    configurable: true,
    enumerable: false,
    value: clearMeasures,
  },
  eventLoopUtilization: {
    configurable: true,
    enumerable: false,
    value: eventLoopUtilization,
  },
  getEntries: {
    configurable: true,
    enumerable: false,
    value: getEntries,
  },
  getEntriesByName: {
    configurable: true,
    enumerable: false,
    value: getEntriesByName,
  },
  getEntriesByType: {
    configurable: true,
    enumerable: false,
    value: getEntriesByType,
  },
  mark: {
    configurable: true,
    enumerable: false,
    value: mark,
  },
  measure: {
    configurable: true,
    enumerable: false,
    value: measure,
  },
  nodeTiming: {
    configurable: true,
    enumerable: false,
    value: nodeTiming,
  },
  now: {
    configurable: true,
    enumerable: false,
    value: now,
  },
  timerify: {
    configurable: true,
    enumerable: false,
    value: timerify,
  },
  // This would be updated during pre-execution in case
  // the process is launched from a snapshot.
  // TODO(joyeecheung): we may want to warn about access to
  // this during snapshot building.
  timeOrigin: {
    configurable: true,
    enumerable: true,
    value: getTimeOriginTimestamp(),
  },
  toJSON: {
    configurable: true,
    enumerable: true,
    value: toJSON,
  }
});

function refreshTimeOrigin() {
  ObjectDefineProperty(Performance.prototype, 'timeOrigin', {
    configurable: true,
    enumerable: true,
    value: getTimeOriginTimestamp(),
  });
}

module.exports = {
  InternalPerformance,
  refreshTimeOrigin
};
