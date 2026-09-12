'use strict';

const {
  MathFloor,
  MathRandom,
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
const { validateThisInternalField } = require('internal/validators');

const { inspect } = require('util');

const kName = Symbol('PerformanceEntry.Name');
const kId = Symbol('PerformanceEntry.Id');
const kEntryType = Symbol('PerformanceEntry.EntryType');
const kStartTime = Symbol('PerformanceEntry.StartTime');
const kDuration = Symbol('PerformanceEntry.Duration');
const kNavigationId = Symbol('PerformanceEntry.NavigationId');
const kDetail = Symbol('NodePerformanceEntry.Detail');
const kSkipThrow = Symbol('kSkipThrow');

let lastPerformanceEntryId = MathFloor(MathRandom() * 9901) + 100;

function nextPerformanceEntryId() {
  return ++lastPerformanceEntryId;
}

function markPerformanceEntryQueued(entry) {
  validateThisInternalField(entry, kName, 'PerformanceEntry');
  if (entry[kId] === 0) {
    entry[kId] = nextPerformanceEntryId();
  }
}

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
    this[kId] = 0;
    this[kEntryType] = type;
    this[kStartTime] = start;
    this[kDuration] = duration;
    this[kNavigationId] = 0;
  }

  get id() {
    validateThisInternalField(this, kName, 'PerformanceEntry');
    return this[kId];
  }

  get name() {
    validateThisInternalField(this, kName, 'PerformanceEntry');
    return this[kName];
  }

  get entryType() {
    validateThisInternalField(this, kEntryType, 'PerformanceEntry');
    return this[kEntryType];
  }

  get startTime() {
    validateThisInternalField(this, kStartTime, 'PerformanceEntry');
    return this[kStartTime];
  }

  get duration() {
    validateThisInternalField(this, kDuration, 'PerformanceEntry');
    return this[kDuration];
  }

  get navigationId() {
    validateThisInternalField(this, kName, 'PerformanceEntry');
    return this[kNavigationId];
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
    validateThisInternalField(this, kName, 'PerformanceEntry');
    return {
      name: this[kName],
      entryType: this[kEntryType],
      startTime: this[kStartTime],
      duration: this[kDuration],
    };
  }
}
ObjectDefineProperties(PerformanceEntry.prototype, {
  id: kEnumerableProperty,
  name: kEnumerableProperty,
  entryType: kEnumerableProperty,
  startTime: kEnumerableProperty,
  duration: kEnumerableProperty,
  navigationId: kEnumerableProperty,
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
    validateThisInternalField(this, kDetail, 'NodePerformanceEntry');
    return this[kDetail];
  }

  toJSON() {
    validateThisInternalField(this, kName, 'PerformanceEntry');
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
  markPerformanceEntryQueued,
  PerformanceNodeEntry,
  createPerformanceNodeEntry,
  kSkipThrow,
};
