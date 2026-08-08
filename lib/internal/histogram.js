'use strict';

const {
  ArrayIsArray,
  Float64Array,
  Map,
  MapPrototypeEntries,
  NumberIsNaN,
  NumberMAX_SAFE_INTEGER,
  ObjectFromEntries,
  Symbol,
} = primordials;

const {
  Histogram: _Histogram,
} = internalBinding('performance');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
} = require('internal/util');

const { inspect } = require('util');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const {
  validateInteger,
  validateNumber,
  validateObject,
} = require('internal/validators');

const kDestroy = Symbol('kDestroy');
const kHandle = Symbol('kHandle');
const kRecordable = Symbol('kRecordable');

const {
  kClone,
  kDeserialize,
  markTransferMode,
} = require('internal/worker/js_transferable');

function isHistogram(object) {
  return object?.[kHandle] !== undefined;
}

const kSkipThrow = Symbol('kSkipThrow');

class Histogram {
  constructor(skipThrowSymbol = undefined) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `Histogram ${inspect({
      min: this.min,
      max: this.max,
      mean: this.mean,
      exceeds: this.exceeds,
      stddev: this.stddev,
      skewness: this.skewness,
      kurtosis: this.kurtosis,
      count: this.count,
      percentiles: this.percentiles,
    }, opts)}`;
  }

  /**
   * @readonly
   * @type {number}
   */
  get count() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.count();
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get countBigInt() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.countBigInt();
  }

  /**
   * Returns the probability that a recorded value will exceed `value`
   * (the complement of the cumulative distribution function).
   * @param {number} value
   * @returns {number} A value between 0.0 and 1.0.
   */
  ccdf(value) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    validateNumber(value, 'value');
    return 1 - this[kHandle]?.cdf(value);
  }

  /**
   * Returns the cumulative distribution function (CDF) value for the
   * given value, representing the probability that a recorded value
   * will be less than or equal to `value`.
   * @param {number} value
   * @returns {number} A value between 0.0 and 1.0.
   */
  cdf(value) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    validateNumber(value, 'value');
    return this[kHandle]?.cdf(value);
  }

  /**
   * Returns the number of recorded values that fall within the
   * equivalent value range of the given value.
   * @param {number} value
   * @returns {number}
   */
  countAt(value) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    validateNumber(value, 'value');
    return this[kHandle]?.countAt(value);
  }

  /**
   * @readonly
   * @type {number}
   */
  get min() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.min();
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get minBigInt() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.minBigInt();
  }

  /**
   * @readonly
   * @type {number}
   */
  get max() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.max();
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get maxBigInt() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.maxBigInt();
  }

  /**
   * @readonly
   * @type {number}
   */
  get mean() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.mean();
  }

  /**
   * @readonly
   * @type {number}
   */
  get exceeds() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.exceeds();
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get exceedsBigInt() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.exceedsBigInt();
  }

  /**
   * Returns the Kolmogorov-Smirnov test statistic comparing this
   * histogram's distribution to another's. Returns a value between
   * 0.0 (identical distributions) and 1.0 (completely disjoint).
   * @param {Histogram} other
   * @returns {number}
   */
  ksTest(other) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    if (!isHistogram(other))
      throw new ERR_INVALID_ARG_TYPE('other', 'Histogram', other);
    return this[kHandle]?.ksTest(other[kHandle]);
  }

  /**
   * Returns the excess kurtosis of the recorded values, a measure of
   * the heaviness of the distribution's tails. A positive value indicates
   * heavier tails (more outliers) than a normal distribution.
   * @readonly
   * @type {number}
   */
  get kurtosis() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.kurtosis();
  }

  /**
   * Returns a {Map} containing the histogram data bucketed into
   * linearly-spaced intervals of `stepSize`.
   * @param {number} stepSize The width of each linear bucket.
   * @returns {Map<number,number>}
   */
  linearBuckets(stepSize) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    validateInteger(stepSize, 'stepSize', 1);
    const map = new Map();
    this[kHandle]?.linearBuckets(stepSize, map);
    return map;
  }

  /**
   * Returns a {Map} containing the histogram data bucketed into
   * logarithmically-spaced intervals.
   * @param {number} firstBucket The value of the first bucket boundary.
   * @param {number} base The logarithmic base for bucket width growth.
   * @returns {Map<number,number>}
   */
  logBuckets(firstBucket, base) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    validateInteger(firstBucket, 'firstBucket', 1);
    validateNumber(base, 'base');
    if (base <= 1)
      throw new ERR_OUT_OF_RANGE('base', '> 1', base);
    const map = new Map();
    this[kHandle]?.logBuckets(firstBucket, base, map);
    return map;
  }

  /**
   * Returns the skewness of the recorded values, a measure of the
   * asymmetry of the distribution. A positive value indicates a
   * right-skewed distribution (longer right tail).
   * @readonly
   * @type {number}
   */
  get skewness() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.skewness();
  }

  /**
   * @readonly
   * @type {number}
   */
  get stddev() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.stddev();
  }

  /**
   * @param {number} percentile
   * @returns {number}
   */
  percentile(percentile) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    validateNumber(percentile, 'percentile');
    if (NumberIsNaN(percentile) || percentile <= 0 || percentile > 100)
      throw new ERR_OUT_OF_RANGE('percentile', '> 0 && <= 100', percentile);

    return this[kHandle]?.percentile(percentile);
  }

  /**
   * @param {number} percentile
   * @returns {bigint}
   */
  percentileBigInt(percentile) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    validateNumber(percentile, 'percentile');
    if (NumberIsNaN(percentile) || percentile <= 0 || percentile > 100)
      throw new ERR_OUT_OF_RANGE('percentile', '> 0 && <= 100', percentile);

    return this[kHandle]?.percentileBigInt(percentile);
  }

  /**
   * @readonly
   * @type {Map<number,number>}
   */
  get percentiles() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    const map = new Map();
    this[kHandle]?.percentiles(map);
    return map;
  }

  /**
   * @readonly
   * @type {Map<number,bigint>}
   */
  get percentilesBigInt() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    const map = new Map();
    this[kHandle]?.percentilesBigInt(map);
    return map;
  }

  /**
   * Returns a {Map} of values at the specified percentiles, computed
   * in a single efficient pass over the histogram.
   * @param {number[]} percentiles Array of percentile values (0, 100].
   * @returns {Map<number,number>}
   */
  percentilesAt(percentiles) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    if (!ArrayIsArray(percentiles))
      throw new ERR_INVALID_ARG_TYPE('percentiles', 'Array', percentiles);
    for (let i = 0; i < percentiles.length; i++) {
      validateNumber(percentiles[i], `percentiles[${i}]`);
      if (NumberIsNaN(percentiles[i]) ||
          percentiles[i] <= 0 || percentiles[i] > 100)
        throw new ERR_OUT_OF_RANGE(
          `percentiles[${i}]`, '> 0 && <= 100', percentiles[i]);
    }
    const sorted = [...percentiles].sort((a, b) => a - b);
    const input = new Float64Array(sorted);
    const map = new Map();
    this[kHandle]?.percentilesAt(map, input);
    return map;
  }

  /**
   * @returns {void}
   */
  reset() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    this[kHandle]?.reset();
  }

  [kClone]() {
    const handle = this[kHandle];
    return {
      data: { handle },
      deserializeInfo: 'internal/histogram:ClonedHistogram',
    };
  }

  [kDeserialize]({ handle }) {
    this[kHandle] = handle;
  }

  toJSON() {
    return {
      count: this.count,
      min: this.min,
      max: this.max,
      mean: this.mean,
      exceeds: this.exceeds,
      stddev: this.stddev,
      skewness: this.skewness,
      kurtosis: this.kurtosis,
      percentiles: ObjectFromEntries(MapPrototypeEntries(this.percentiles)),
    };
  }
}

class RecordableHistogram extends Histogram {
  constructor(skipThrowSymbol = undefined) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    super(skipThrowSymbol);
  }

  /**
   * @param {number|bigint} val
   * @returns {void}
   */
  record(val) {
    if (this[kRecordable] === undefined)
      throw new ERR_INVALID_THIS('RecordableHistogram');
    if (typeof val === 'bigint') {
      this[kHandle]?.record(val);
      return;
    }

    validateInteger(val, 'val', 1);

    this[kHandle]?.record(val);
  }

  /**
   * @returns {void}
   */
  recordDelta() {
    if (this[kRecordable] === undefined)
      throw new ERR_INVALID_THIS('RecordableHistogram');
    this[kHandle]?.recordDelta();
  }

  /**
   * Records a value with coordinated omission correction, backfilling
   * intermediate values at `expectedInterval` steps between the last
   * recorded value and `val`. This compensates for measurement gaps
   * caused by the system being stalled.
   * @param {number|bigint} val The amount to record.
   * @param {number|bigint} expectedInterval The expected recording interval.
   * @returns {void}
   */
  recordCorrected(val, expectedInterval) {
    if (this[kRecordable] === undefined)
      throw new ERR_INVALID_THIS('RecordableHistogram');
    if (typeof val === 'bigint') {
      if (typeof expectedInterval !== 'bigint')
        throw new ERR_INVALID_ARG_TYPE(
          'expectedInterval', 'bigint', expectedInterval);
      this[kHandle]?.recordCorrected(val, expectedInterval);
      return;
    }
    validateInteger(val, 'val', 1);
    validateInteger(expectedInterval, 'expectedInterval', 1);
    this[kHandle]?.recordCorrected(val, expectedInterval);
  }

  /**
   * Subtracts the values of `other` from this histogram. Both
   * histograms must have compatible configurations. Counts that would
   * become negative are clamped to zero.
   * @param {RecordableHistogram} other
   */
  subtract(other) {
    if (this[kRecordable] === undefined)
      throw new ERR_INVALID_THIS('RecordableHistogram');
    if (other[kRecordable] === undefined)
      throw new ERR_INVALID_ARG_TYPE('other', 'RecordableHistogram', other);
    this[kHandle]?.subtract(other[kHandle]);
  }

  /**
   * @param {RecordableHistogram} other
   */
  add(other) {
    if (this[kRecordable] === undefined)
      throw new ERR_INVALID_THIS('RecordableHistogram');
    if (other[kRecordable] === undefined)
      throw new ERR_INVALID_ARG_TYPE('other', 'RecordableHistogram', other);
    this[kHandle]?.add(other[kHandle]);
  }

  [kClone]() {
    const handle = this[kHandle];
    return {
      data: { handle },
      deserializeInfo: 'internal/histogram:ClonedRecordableHistogram',
    };
  }

  [kDeserialize]({ handle }) {
    this[kHandle] = handle;
  }
}

function ClonedHistogram(handle) {
  const histogram = new Histogram(kSkipThrow);
  markTransferMode(histogram, true, false);
  histogram[kHandle] = handle;
  return histogram;
}

ClonedHistogram.prototype[kDeserialize] = () => { };

function ClonedRecordableHistogram(handle) {
  const histogram = new RecordableHistogram(kSkipThrow);

  markTransferMode(histogram, true, false);
  histogram[kRecordable] = true;
  histogram[kHandle] = handle;
  histogram.constructor = RecordableHistogram;

  return histogram;
}

ClonedRecordableHistogram.prototype[kDeserialize] = () => { };

function createRecordableHistogram(handle) {
  return new ClonedRecordableHistogram(handle);
}

/**
 * @param {{
 *   lowest? : number,
 *   highest? : number,
 *   figures? : number
 * }} [options]
 * @returns {RecordableHistogram}
 */
function createHistogram(options = kEmptyObject) {
  validateObject(options, 'options');
  const {
    lowest = 1,
    highest = NumberMAX_SAFE_INTEGER,
    figures = 3,
  } = options;
  if (typeof lowest !== 'bigint')
    validateInteger(lowest, 'options.lowest', 1, NumberMAX_SAFE_INTEGER);
  if (typeof highest !== 'bigint') {
    validateInteger(highest, 'options.highest',
                    2 * lowest, NumberMAX_SAFE_INTEGER);
  } else if (highest < 2n * lowest) {
    throw new ERR_INVALID_ARG_VALUE.RangeError('options.highest', highest);
  }
  validateInteger(figures, 'options.figures', 1, 5);
  return createRecordableHistogram(new _Histogram(lowest, highest, figures));
}

module.exports = {
  Histogram,
  RecordableHistogram,
  ClonedHistogram,
  ClonedRecordableHistogram,
  isHistogram,
  kDestroy,
  kHandle,
  kSkipThrow,
  createHistogram,
};
