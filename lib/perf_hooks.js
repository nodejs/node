'use strict';

const {
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  TypeError,
} = primordials;

const {
  timeOriginTimestamp,
  constants,
} = internalBinding('performance');

const {
  EventTarget,
} = require('internal/event_target');

const {
  PerformanceEntry,
  now,
} = require('internal/perf/perf');
const { PerformanceObserver } = require('internal/perf/observe');

const {
  PerformanceMark,
  mark,
  measure,
  clearMarks,
} = require('internal/perf/usertiming');

const {
  createHistogram
} = require('internal/histogram');

const eventLoopUtilization = require('internal/perf/event_loop_utilization');
const monitorEventLoopDelay = require('internal/perf/event_loop_delay');
const nodeTiming = require('internal/perf/nodetiming');
const timerify = require('internal/perf/timerify');
const { customInspectSymbol: kInspect } = require('internal/util');
const { inspect } = require('util');

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

class InternalPerformance extends EventTarget {}
InternalPerformance.prototype.constructor = Performance.prototype.constructor;
ObjectSetPrototypeOf(InternalPerformance.prototype, Performance.prototype);

ObjectDefineProperties(Performance.prototype, {
  clearMarks: {
    configurable: true,
    enumerable: false,
    value: clearMarks,
  },
  eventLoopUtilization: {
    configurable: true,
    enumerable: false,
    value: eventLoopUtilization,
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
  timeOrigin: {
    configurable: true,
    enumerable: true,
    value: timeOriginTimestamp,
  }
});

module.exports = {
  PerformanceEntry,
  PerformanceMark,
  PerformanceObserver,
  monitorEventLoopDelay,
  createHistogram,
  performance: new InternalPerformance(),
};

ObjectDefineProperty(module.exports, 'constants', {
  configurable: false,
  enumerable: true,
  value: constants
});
