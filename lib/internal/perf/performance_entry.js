'use strict';

const {
  ObjectDefineProperties,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
  },
} = require('internal/errors');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
} = require('internal/util');
const { validateInternalField } = require('internal/validators');

const { inspect } = require('util');

const kName = Symbol('PerformanceEntry.Name');
const kEntryType = Symbol('PerformanceEntry.EntryType');
const kStartTime = Symbol('PerformanceEntry.StartTime');
const kDuration = Symbol('PerformanceEntry.Duration');
const kDetail = Symbol('NodePerformanceEntry.Detail');
const kSkipThrow = Symbol('kSkipThrow');

function isPerformanceEntry(obj) {
  return obj?.[kName] !== undefined;
}

class PerformanceEntry {
  constructor(
    skipThrowSymbol = undefined,
    name = undefined,
    type = undefined,
    start = undefined,
    duration = undefined,
  ) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    this[kName] = name;
    this[kEntryType] = type;
    this[kStartTime] = start;
    this[kDuration] = duration;
  }

  get name() {
    validateInternalField(this, kName, 'PerformanceEntry');
    return this[kName];
  }

  get entryType() {
    validateInternalField(this, kEntryType, 'PerformanceEntry');
    return this[kEntryType];
  }

  get startTime() {
    validateInternalField(this, kStartTime, 'PerformanceEntry');
    return this[kStartTime];
  }

  get duration() {
    validateInternalField(this, kDuration, 'PerformanceEntry');
    return this[kDuration];
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this.constructor.name} ${inspect(this.toJSON(), opts)}`;
  }

  toJSON() {
    validateInternalField(this, kName, 'PerformanceEntry');
    return {
      name: this[kName],
      entryType: this[kEntryType],
      startTime: this[kStartTime],
      duration: this[kDuration],
    };
  }
}
ObjectDefineProperties(PerformanceEntry.prototype, {
  name: kEnumerableProperty,
  entryType: kEnumerableProperty,
  startTime: kEnumerableProperty,
  duration: kEnumerableProperty,
  toJSON: kEnumerableProperty,
});

function createPerformanceEntry(name, type, start, duration) {
  return new PerformanceEntry(kSkipThrow, name, type, start, duration);
}

/**
 * Node.js specific extension to PerformanceEntry.
 */
class PerformanceNodeEntry extends PerformanceEntry {
  get detail() {
    validateInternalField(this, kDetail, 'NodePerformanceEntry');
    return this[kDetail];
  }

  toJSON() {
    validateInternalField(this, kName, 'PerformanceEntry');
    return {
      name: this[kName],
      entryType: this[kEntryType],
      startTime: this[kStartTime],
      duration: this[kDuration],
      detail: this[kDetail],
    };
  }
}

function createPerformanceNodeEntry(name, type, start, duration, detail) {
  const entry = new PerformanceNodeEntry(kSkipThrow, name, type, start, duration);

  entry[kDetail] = detail;

  return entry;
}

module.exports = {
  createPerformanceEntry,
  PerformanceEntry,
  isPerformanceEntry,
  PerformanceNodeEntry,
  createPerformanceNodeEntry,
  kSkipThrow,
};
