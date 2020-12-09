'use strict';

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const { format } = require('util');
const { SafeMap, Symbol } = primordials;

const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
} = require('internal/errors').codes;

const kDestroy = Symbol('kDestroy');
const kHandle = Symbol('kHandle');

// Histograms are created internally by Node.js and used to
// record various metrics. This Histogram class provides a
// generally read-only view of the internal histogram.
class Histogram {
  #map = new SafeMap();

  constructor(internal) {
    this[kHandle] = internal;
  }

  [kInspect]() {
    const obj = {
      min: this.min,
      max: this.max,
      mean: this.mean,
      exceeds: this.exceeds,
      stddev: this.stddev,
      percentiles: this.percentiles,
    };
    return `Histogram ${format(obj)}`;
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
    if (typeof percentile !== 'number')
      throw new ERR_INVALID_ARG_TYPE('percentile', 'number', percentile);

    if (percentile <= 0 || percentile > 100)
      throw new ERR_INVALID_ARG_VALUE.RangeError('percentile', percentile);

    return this[kHandle]?.percentile(percentile);
  }

  get percentiles() {
    this.#map.clear();
    this[kHandle]?.percentiles(this.#map);
    return this.#map;
  }

  reset() {
    this[kHandle]?.reset();
  }

  [kDestroy]() {
    this[kHandle] = undefined;
  }
}

module.exports = {
  Histogram,
  kDestroy,
  kHandle,
};
