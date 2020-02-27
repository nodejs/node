'use strict';

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const { format } = require('util');
const { Map, Symbol } = primordials;

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
  #handle = undefined;
  #map = new Map();

  constructor(internal) {
    this.#handle = internal;
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
    return this.#handle ? this.#handle.min() : undefined;
  }

  get max() {
    return this.#handle ? this.#handle.max() : undefined;
  }

  get mean() {
    return this.#handle ? this.#handle.mean() : undefined;
  }

  get exceeds() {
    return this.#handle ? this.#handle.exceeds() : undefined;
  }

  get stddev() {
    return this.#handle ? this.#handle.stddev() : undefined;
  }

  percentile(percentile) {
    if (typeof percentile !== 'number')
      throw new ERR_INVALID_ARG_TYPE('percentile', 'number', percentile);

    if (percentile <= 0 || percentile > 100)
      throw new ERR_INVALID_ARG_VALUE.RangeError('percentile', percentile);

    return this.#handle ? this.#handle.percentile(percentile) : undefined;
  }

  get percentiles() {
    this.#map.clear();
    if (this.#handle)
      this.#handle.percentiles(this.#map);
    return this.#map;
  }

  reset() {
    if (this.#handle)
      this.#handle.reset();
  }

  [kDestroy]() {
    this.#handle = undefined;
  }

  get [kHandle]() { return this.#handle; }
}

module.exports = {
  Histogram,
  kDestroy,
  kHandle,
};
