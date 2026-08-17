'use strict';

const {
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
  validateArray,
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
   * Returns the exponentially weighted moving average of recorded values.
   * Only active when the histogram was created with a `halfLife` option.
   * Returns 0 when EWMA is not enabled or no values have been recorded.
   * @readonly
   * @type {number}
   */
  get ewmaMean() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.ewmaMean();
  }

  /**
   * Returns the exponentially weighted moving standard deviation.
   * Only active when the histogram was created with a `halfLife` option.
   * Returns 0 when EWMA is not enabled or no values have been recorded.
   * @readonly
   * @type {number}
   */
  get ewmaStddev() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.ewmaStddev();
  }

  /**
   * Returns the EWMA-smoothed error rate: the probability of a recorded
   * value exceeding the configured `threshold`. Only active when the
   * histogram was created with both `halfLife` and `threshold` options.
   * Returns 0 when not enabled or no values have been recorded.
   * @readonly
   * @type {number}
   */
  get ewmaErrorRate() {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    return this[kHandle]?.ewmaErrorRate();
  }

  /**
   * Returns the SLO burn rate: how fast the error budget is being consumed.
   * A burn rate of 1 means the budget will be exactly exhausted over the
   * SLO window. A burn rate of 10 means it is being consumed 10x faster.
   * Requires `halfLife` and `threshold` to be configured.
   * @param {number} sloTarget - The SLO target as a fraction (e.g. 0.999
   *   for 99.9%).
   * @returns {number}
   */
  burnRate(sloTarget) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    validateNumber(sloTarget, 'sloTarget');
    if (NumberIsNaN(sloTarget) || sloTarget <= 0 || sloTarget >= 1)
      throw new ERR_OUT_OF_RANGE('sloTarget', '> 0 && < 1', sloTarget);
    const errorRate = this[kHandle]?.ewmaErrorRate();
    if (errorRate === undefined) return undefined;
    const errorBudget = 1 - sloTarget;
    return errorRate / errorBudget;
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
   * Performs Welch's t-test comparing this histogram to another.
   * Returns an object with the t-statistic, degrees of freedom,
   * two-tailed p-value, and confidence interval on the difference
   * of means.
   * @param {Histogram} other
   * @param {{ confidence?: number }} [options]
   * @returns {{ tStatistic: number, degreesOfFreedom: number,
   *             pValue: number,
   *             confidenceInterval: { lower: number, upper: number } }}
   */
  welchTest(other, options = kEmptyObject) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    if (!isHistogram(other))
      throw new ERR_INVALID_ARG_TYPE('other', 'Histogram', other);
    validateObject(options, 'options');
    const { confidence = 0.95 } = options;
    validateNumber(confidence, 'options.confidence');
    if (NumberIsNaN(confidence) || confidence <= 0 || confidence >= 1)
      throw new ERR_OUT_OF_RANGE('options.confidence',
                                 '> 0 && < 1', confidence);
    const result = this[kHandle]?.welchTest(other[kHandle], confidence);
    if (result === undefined) return undefined;
    return {
      __proto__: null,
      tStatistic: result[0],
      degreesOfFreedom: result[1],
      pValue: result[2],
      confidenceInterval: {
        __proto__: null,
        lower: result[3],
        upper: result[4],
      },
    };
  }

  /**
   * Performs a Mann-Whitney U test comparing this histogram to
   * another. Returns an object with the U statistic, z-score,
   * and two-tailed p-value (normal approximation).
   * @param {Histogram} other
   * @returns {{ uStatistic: number, zScore: number, pValue: number }}
   */
  mannWhitneyTest(other) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    if (!isHistogram(other))
      throw new ERR_INVALID_ARG_TYPE('other', 'Histogram', other);
    const result = this[kHandle]?.mannWhitneyTest(other[kHandle]);
    if (result === undefined) return undefined;
    return {
      __proto__: null,
      uStatistic: result[0],
      zScore: result[1],
      pValue: result[2],
    };
  }

  /**
   * Computes Cohen's d effect size comparing this histogram to
   * another. Uses the pooled standard deviation. Positive values
   * indicate this histogram has a higher mean.
   * @param {Histogram} other
   * @returns {number}
   */
  cohensD(other) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    if (!isHistogram(other))
      throw new ERR_INVALID_ARG_TYPE('other', 'Histogram', other);
    return this[kHandle]?.cohensD(other[kHandle]);
  }

  /**
   * Computes Cliff's delta comparing this histogram to another.
   * Returns a value between -1 and 1. Positive values indicate
   * this histogram tends to produce larger values.
   * @param {Histogram} other
   * @returns {number}
   */
  cliffsD(other) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    if (!isHistogram(other))
      throw new ERR_INVALID_ARG_TYPE('other', 'Histogram', other);
    return this[kHandle]?.cliffsD(other[kHandle]);
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
   * Returns a confidence interval for the given percentile using the
   * exact binomial method. The result contains the point estimate and
   * the lower/upper bounds of the interval.
   * @param {number} percentile
   * @param {{ confidence?: number }} [options]
   * @returns {{ value: number, lower: number, upper: number }}
   */
  percentileCI(percentile, options = kEmptyObject) {
    if (!isHistogram(this))
      throw new ERR_INVALID_THIS('Histogram');
    validateNumber(percentile, 'percentile');
    if (NumberIsNaN(percentile) || percentile <= 0 || percentile > 100)
      throw new ERR_OUT_OF_RANGE('percentile', '> 0 && <= 100', percentile);
    validateObject(options, 'options');
    const { confidence = 0.95 } = options;
    validateNumber(confidence, 'options.confidence');
    if (NumberIsNaN(confidence) || confidence <= 0 || confidence >= 1)
      throw new ERR_OUT_OF_RANGE('options.confidence',
                                 '> 0 && < 1', confidence);
    const result = this[kHandle]?.percentileCI(percentile, confidence);
    if (result === undefined) return undefined;
    return {
      __proto__: null,
      value: result[0],
      lower: result[1],
      upper: result[2],
    };
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
    validateArray(percentiles, 'percentiles');
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
    const json = {
      count: this.count,
      min: this.min,
      max: this.max,
      mean: this.mean,
      exceeds: this.exceeds,
      stddev: this.stddev,
      skewness: this.skewness,
      kurtosis: this.kurtosis,
      ewmaMean: this.ewmaMean,
      ewmaStddev: this.ewmaStddev,
      ewmaErrorRate: this.ewmaErrorRate,
      percentiles: ObjectFromEntries(MapPrototypeEntries(this.percentiles)),
    };
    return json;
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
 *   figures? : number,
 *   halfLife? : number,
 *   threshold? : number
 * }} [options]
 * @returns {RecordableHistogram}
 */
function createHistogram(options = kEmptyObject) {
  validateObject(options, 'options');
  const {
    lowest = 1,
    highest = NumberMAX_SAFE_INTEGER,
    figures = 3,
    halfLife = 0,
    threshold = 0,
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
  validateNumber(halfLife, 'options.halfLife');
  if (halfLife < 0)
    throw new ERR_OUT_OF_RANGE('options.halfLife', '>= 0', halfLife);
  validateNumber(threshold, 'options.threshold');
  if (threshold < 0)
    throw new ERR_OUT_OF_RANGE('options.threshold', '>= 0', threshold);
  return createRecordableHistogram(
    new _Histogram(lowest, highest, figures, halfLife, threshold));
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
