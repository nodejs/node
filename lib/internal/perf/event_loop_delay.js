'use strict';
const {
  Symbol,
  SymbolDispose,
} = primordials;

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  createELDHistogram,
} = internalBinding('performance');

const {
  validateBoolean,
  validateInteger,
  validateObject,
} = require('internal/validators');

const {
  Histogram,
  kHandle,
  kSkipThrow,
} = require('internal/histogram');

const {
  kEmptyObject,
} = require('internal/util');

const {
  markTransferMode,
} = require('internal/worker/js_transferable');

const kEnabled = Symbol('kEnabled');

class ELDHistogram extends Histogram {
  constructor(skipThrowSymbol = undefined) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    super(skipThrowSymbol);
  }

  /**
   * @returns {boolean}
   */
  enable() {
    if (this[kEnabled] === undefined)
      throw new ERR_INVALID_THIS('ELDHistogram');
    if (this[kEnabled]) return false;
    this[kEnabled] = true;
    this[kHandle].start();
    return true;
  }

  /**
   * @returns {boolean}
   */
  disable() {
    if (this[kEnabled] === undefined)
      throw new ERR_INVALID_THIS('ELDHistogram');
    if (!this[kEnabled]) return false;
    this[kEnabled] = false;
    this[kHandle].stop();
    return true;
  }

  [SymbolDispose]() {
    this.disable();
  }
}

/**
 * @param {{
 *   samplePerIteration : boolean,
 *   resolution : number
 * }} [options]
 * @returns {ELDHistogram}
 */
function monitorEventLoopDelay(options = kEmptyObject) {
  validateObject(options, 'options');

  const { samplePerIteration = false, resolution = 10 } = options;
  validateBoolean(samplePerIteration, 'options.samplePerIteration');
  validateInteger(resolution, 'options.resolution', 1);

  const histogram = new ELDHistogram(kSkipThrow);
  markTransferMode(histogram, true, false);
  histogram[kEnabled] = false;
  histogram[kHandle] = createELDHistogram(resolution, samplePerIteration);
  return histogram;
}

module.exports = monitorEventLoopDelay;
