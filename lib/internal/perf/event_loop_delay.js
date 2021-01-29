'use strict';
const {
  Symbol,
  TypeError,
} = primordials;

const {
  ELDHistogram: _ELDHistogram,
} = internalBinding('performance');

const {
  validateInteger,
  validateObject,
} = require('internal/validators');

const {
  Histogram,
  kHandle,
} = require('internal/histogram');

const kEnabled = Symbol('kEnabled');

class ELDHistogram extends Histogram {
  constructor(i) {
    if (!(i instanceof _ELDHistogram)) {
      // eslint-disable-next-line no-restricted-syntax
      throw new TypeError('illegal constructor');
    }
    super(i);
    this[kEnabled] = false;
  }
  enable() {
    if (this[kEnabled]) return false;
    this[kEnabled] = true;
    this[kHandle].start();
    return true;
  }
  disable() {
    if (!this[kEnabled]) return false;
    this[kEnabled] = false;
    this[kHandle].stop();
    return true;
  }
}

function monitorEventLoopDelay(options = {}) {
  validateObject(options, 'options');

  const { resolution = 10 } = options;
  validateInteger(resolution, 'options.resolution', 1);

  return new ELDHistogram(new _ELDHistogram(resolution));
}

module.exports = monitorEventLoopDelay;
