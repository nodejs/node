'use strict';

const {
  ObjectSetPrototypeOf,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
  }
} = require('internal/errors');

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const { inspect } = require('util');

const kName = Symbol('kName');
const kType = Symbol('kType');
const kStart = Symbol('kStart');
const kDuration = Symbol('kDuration');
const kDetail = Symbol('kDetail');

function isPerformanceEntry(obj) {
  return obj?.[kName] !== undefined;
}

class PerformanceEntry {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }

  get name() { return this[kName]; }

  get entryType() { return this[kType]; }

  get startTime() { return this[kStart]; }

  get duration() { return this[kDuration]; }

  get detail() { return this[kDetail]; }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `${this.constructor.name} ${inspect(this.toJSON(), opts)}`;
  }

  toJSON() {
    return {
      name: this.name,
      entryType: this.entryType,
      startTime: this.startTime,
      duration: this.duration,
      detail: this.detail,
    };
  }
}

class InternalPerformanceEntry {
  constructor(name, type, start, duration, detail) {
    this[kName] = name;
    this[kType] = type;
    this[kStart] = start;
    this[kDuration] = duration;
    this[kDetail] = detail;
  }
}

InternalPerformanceEntry.prototype.constructor = PerformanceEntry;
ObjectSetPrototypeOf(
  InternalPerformanceEntry.prototype,
  PerformanceEntry.prototype);

module.exports = {
  InternalPerformanceEntry,
  PerformanceEntry,
  isPerformanceEntry,
};
