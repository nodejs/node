'use strict';

const {
  NumberIsNaN,
  NumberIsInteger,
  NumberMAX_SAFE_INTEGER,
  ObjectSetPrototypeOf,
  SafeMap,
  Symbol,
  TypeError,
} = primordials;

const {
  Histogram: _Histogram
} = internalBinding('performance');

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const { inspect } = require('util');

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_ARG_TYPE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const {
  validateNumber,
} = require('internal/validators');

const kDestroy = Symbol('kDestroy');
const kHandle = Symbol('kHandle');
const kMap = Symbol('kMap');

const {
  kClone,
  kDeserialize,
  JSTransferable,
} = require('internal/worker/js_transferable');

function isHistogram(object) {
  return object?.[kHandle] !== undefined;
}

class Histogram extends JSTransferable {
  constructor(internal) {
    super();
    this[kHandle] = internal;
    this[kMap] = new SafeMap();
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `Histogram ${inspect({
      min: this.min,
      max: this.max,
      mean: this.mean,
      exceeds: this.exceeds,
      stddev: this.stddev,
      percentiles: this.percentiles,
    }, opts)}`;
  }

  get min() {
    return this[kHandle]?.min();
  }

  get max() {
    return this[kHandle]?.max();
  }

  get mean() {
    return this[kHandle]?.mean();
  }

  get exceeds() {
    return this[kHandle]?.exceeds();
  }

  get stddev() {
    return this[kHandle]?.stddev();
  }

  percentile(percentile) {
    validateNumber(percentile, 'percentile');

    if (NumberIsNaN(percentile) || percentile <= 0 || percentile > 100)
      throw new ERR_INVALID_ARG_VALUE.RangeError('percentile', percentile);

    return this[kHandle]?.percentile(percentile);
  }

  get percentiles() {
    this[kMap].clear();
    this[kHandle]?.percentiles(this[kMap]);
    return this[kMap];
  }

  reset() {
    this[kHandle]?.reset();
  }

  [kDestroy]() {
    this[kHandle] = undefined;
  }

  [kClone]() {
    const handle = this[kHandle];
    return {
      data: { handle },
      deserializeInfo: 'internal/histogram:InternalHistogram'
    };
  }

  [kDeserialize]({ handle }) {
    this[kHandle] = handle;
  }
}

class RecordableHistogram extends Histogram {
  constructor() {
    // eslint-disable-next-line no-restricted-syntax
    throw new TypeError('illegal constructor');
  }

  record(val) {
    if (typeof val === 'bigint') {
      this[kHandle]?.record(val);
      return;
    }

    if (!NumberIsInteger(val))
      throw new ERR_INVALID_ARG_TYPE('val', ['integer', 'bigint'], val);

    if (val < 1 || val > NumberMAX_SAFE_INTEGER)
      throw new ERR_OUT_OF_RANGE('val', 'a safe integer greater than 0', val);

    this[kHandle]?.record(val);
  }

  recordDelta() {
    this[kHandle]?.recordDelta();
  }

  [kClone]() {
    const handle = this[kHandle];
    return {
      data: { handle },
      deserializeInfo: 'internal/histogram:InternalRecordableHistogram'
    };
  }
}

class InternalHistogram extends JSTransferable {
  constructor(handle) {
    super();
    this[kHandle] = handle;
    this[kMap] = new SafeMap();
  }
}

class InternalRecordableHistogram extends JSTransferable {
  constructor(handle) {
    super();
    this[kHandle] = handle;
    this[kMap] = new SafeMap();
  }
}

InternalHistogram.prototype.constructor = Histogram;
ObjectSetPrototypeOf(
  InternalHistogram.prototype,
  Histogram.prototype);

InternalRecordableHistogram.prototype.constructor = RecordableHistogram;
ObjectSetPrototypeOf(
  InternalRecordableHistogram.prototype,
  RecordableHistogram.prototype);

function createHistogram() {
  return new InternalRecordableHistogram(new _Histogram());
}

module.exports = {
  Histogram,
  RecordableHistogram,
  InternalHistogram,
  InternalRecordableHistogram,
  isHistogram,
  kDestroy,
  kHandle,
  createHistogram,
};
