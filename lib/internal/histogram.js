'use strict';

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const { format } = require('util');

const kHandle = Symbol('kHandle');
const kDestroy = Symbol('kDestroy');

class Histogram {
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
    return this[kHandle] ? this[kHandle].min() : undefined;
  }

  get max() {
    return this[kHandle] ? this[kHandle].max() : undefined;
  }

  get mean() {
    return this[kHandle] ? this[kHandle].mean() : undefined;
  }

  get exceeds() {
    return this[kHandle] ? this[kHandle].exceeds() : undefined;
  }

  get stddev() {
    return this[kHandle] ? this[kHandle].stddev() : undefined;
  }

  percentile(percentile) {
    return this[kHandle] ? this[kHandle].percentile(percentile) : undefined;
  }

  get percentiles() {
    const map = new Map();
    if (this[kHandle])
      this[kHandle].percentiles(map);
    return map;
  }

  reset() {
    if (this[kHandle])
      this[kHandle].reset();
  }

  [kDestroy]() {
    this[kHandle] = undefined;
  }
}

module.exports = { Histogram, kDestroy: kDestroy };
