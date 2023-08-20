'use strict';

const {
  ObjectDefineProperties,
  ReflectConstruct,
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

function isPerformanceEntry(obj) {
  return obj?.[kName] !== undefined;
}

class PerformanceEntry {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
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

function initPerformanceEntry(entry, name, type, start, duration) {
  entry[kName] = name;
  entry[kEntryType] = type;
  entry[kStartTime] = start;
  entry[kDuration] = duration;
}

function createPerformanceEntry(name, type, start, duration) {
  return ReflectConstruct(function PerformanceEntry() {
    initPerformanceEntry(this, name, type, start, duration);
  }, [], PerformanceEntry);
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
  return ReflectConstruct(function PerformanceNodeEntry() {
    initPerformanceEntry(this, name, type, start, duration);
    this[kDetail] = detail;
  }, [], PerformanceNodeEntry);
}

module.exports = {
  initPerformanceEntry,
  createPerformanceEntry,
  PerformanceEntry,
  isPerformanceEntry,
  PerformanceNodeEntry,
  createPerformanceNodeEntry,
};
