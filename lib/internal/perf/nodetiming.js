'use strict';

const {
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
  SafeArrayIterator,
  SafeSet,
} = primordials;

const {
  PerformanceEntry,
  kReadOnlyAttributes,
  now,
} = require('internal/perf/perf');

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const { inspect } = require('util');

const {
  constants: {
    NODE_PERFORMANCE_MILESTONE_NODE_START,
    NODE_PERFORMANCE_MILESTONE_V8_START,
    NODE_PERFORMANCE_MILESTONE_LOOP_START,
    NODE_PERFORMANCE_MILESTONE_LOOP_EXIT,
    NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE,
    NODE_PERFORMANCE_MILESTONE_ENVIRONMENT
  },
  loopIdleTime,
  milestones,
  timeOrigin,
} = internalBinding('performance');

function getMilestoneTimestamp(milestoneIdx) {
  const ns = milestones[milestoneIdx];
  if (ns === -1)
    return ns;
  return ns / 1e6 - timeOrigin;
}

const readOnlyAttributes = new SafeSet(new SafeArrayIterator([
  'nodeStart',
  'v8Start',
  'environment',
  'loopStart',
  'loopExit',
  'bootstrapComplete',
]));

class PerformanceNodeTiming {
  constructor() {
    ObjectDefineProperties(this, {
      name: {
        enumerable: true,
        configurable: true,
        value: 'node'
      },

      entryType: {
        enumerable: true,
        configurable: true,
        value: 'node'
      },

      startTime: {
        enumerable: true,
        configurable: true,
        value: 0
      },

      duration: {
        enumerable: true,
        configurable: true,
        get: now
      },

      nodeStart: {
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_NODE_START);
        }
      },

      v8Start: {
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_V8_START);
        }
      },

      environment: {
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_ENVIRONMENT);
        }
      },

      loopStart: {
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_LOOP_START);
        }
      },

      loopExit: {
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneTimestamp(NODE_PERFORMANCE_MILESTONE_LOOP_EXIT);
        }
      },

      bootstrapComplete: {
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneTimestamp(
            NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE);
        }
      },

      idleTime: {
        enumerable: true,
        configurable: true,
        get: loopIdleTime,
      }
    });
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `PerformanceNodeTiming ${inspect(this.toJSON(), opts)}`;
  }

  toJSON() {
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
      idleTime: this.idleTime,
    };
  }

  static get [kReadOnlyAttributes]() {
    return readOnlyAttributes;
  }
}

ObjectSetPrototypeOf(
  PerformanceNodeTiming.prototype,
  PerformanceEntry.prototype);

module.exports = new PerformanceNodeTiming();
