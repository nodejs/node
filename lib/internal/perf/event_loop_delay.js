'use strict';
const {
  BigInt,
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
  kMaxInt64,
  kSkipThrow,
  validateHistogramOptions,
} = require('internal/histogram');

const {
  kEmptyObject,
} = require('internal/util');

const {
  markTransferMode,
} = require('internal/worker/js_transferable');

const kEnabled = Symbol('kEnabled');

// Default histogram options. The lowest discernible delay is in nanoseconds,
// and its default depends on the sampling mode.
const kDefaultIntervalLowest = 1000;
const kDefaultIterationLowest = 1;
const kDefaultFigures = 3;

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
 *   samplePerIteration? : boolean,
 *   resolution? : number,
 *   lowest? : number|bigint,
 *   highest? : number|bigint,
 *   figures? : number,
 * }} [options]
 * @returns {ELDHistogram}
 */
function monitorEventLoopDelay(options = kEmptyObject) {
  validateObject(options, 'options');

  const { samplePerIteration = false, resolution = 10 } = options;
  validateBoolean(samplePerIteration, 'options.samplePerIteration');
  validateInteger(resolution, 'options.resolution', 1);

  const {
    lowest = samplePerIteration ?
      kDefaultIterationLowest : kDefaultIntervalLowest,
    highest = kMaxInt64,
    figures = kDefaultFigures,
  } = options;
  validateHistogramOptions(lowest, highest, figures);

  // Throws if the native histogram cannot be created with these options.
  const handle = createELDHistogram(
    resolution, samplePerIteration, BigInt(lowest), BigInt(highest), figures);

  const histogram = new ELDHistogram(kSkipThrow);
  markTransferMode(histogram, true, false);
  histogram[kEnabled] = false;
  histogram[kHandle] = handle;
  return histogram;
}

module.exports = monitorEventLoopDelay;
