'use strict';

const { validateNumber } = require('internal/validators');
const { codes: { ERR_OUT_OF_RANGE } } = require('internal/errors');
const {
  MathMin,
  MathMax,
  MathCeil,
  MathSqrt,
  MathPow,
  MathFloor,
  NumberMAX_SAFE_INTEGER,
  ArrayPrototypeSort,
  ArrayPrototypePush,
  ArrayPrototypeFilter,
  ArrayPrototypeReduce,
  ArrayPrototypeSlice,
  NumberIsNaN,
  Symbol,
} = primordials;

const kStatisticalHistogramRecord = Symbol('kStatisticalHistogramRecord');
const kStatisticalHistogramFinish = Symbol('kStatisticalHistogramFinish');

class StatisticalHistogram {
  /**
   * @type {number[]}
   * @default []
   */
  #all = [];
  /**
   * @type {number}
   */
  #min;
  /**
   * @type {number}
   */
  #max;
  /**
   * @type {number}
   */
  #mean;
  /**
   * @type {number}
   */
  #cv;

  /**
   * @returns {number[]}
   */
  get samples() {
    return ArrayPrototypeSlice(this.#all);
  }

  /**
   * @returns {number}
   */
  get min() {
    return this.#min;
  }

  /**
   * @returns {number}
   */
  get max() {
    return this.#max;
  }

  /**
   * @returns {number}
   */
  get mean() {
    return this.#mean;
  }

  /**
   * @returns {number}
   */
  get cv() {
    return this.#cv;
  }

  /**
   * @param {number} percentile
   * @returns {number}
   */
  percentile(percentile) {
    validateNumber(percentile, 'percentile');

    if (NumberIsNaN(percentile) || percentile < 0 || percentile > 100)
      throw new ERR_OUT_OF_RANGE('percentile', '>= 0 && <= 100', percentile);

    if (this.#all.length === 0)
      return 0;

    if (percentile === 0)
      return this.#all[0];

    return this.#all[MathCeil(this.#all.length * (percentile / 100)) - 1];
  }

  /**
   * @param {number} value
   */
  [kStatisticalHistogramRecord](value) {
    validateNumber(value, 'value', 0);

    ArrayPrototypePush(this.#all, value);
  }

  [kStatisticalHistogramFinish]() {
    this.#removeOutliers();

    this.#calculateMinMax();
    this.#calculateMean();
    this.#calculateCv();
  }

  /**
   * References:
   * - https://gist.github.com/rmeissn/f5b42fb3e1386a46f60304a57b6d215a
   * - https://en.wikipedia.org/wiki/Interquartile_range
   */
  #removeOutliers() {
    ArrayPrototypeSort(this.#all, (a, b) => a - b);

    const size = this.#all.length;

    if (size < 4)
      return;

    let q1, q3;

    if ((size - 1) / 4 % 1 === 0 || size / 4 % 1 === 0) {
      q1 = 1 / 2 * (this.#all[MathFloor(size / 4) - 1] + this.#all[MathFloor(size / 4)]);
      q3 = 1 / 2 * (this.#all[MathCeil(size * 3 / 4) - 1] + this.#all[MathCeil(size * 3 / 4)]);
    } else {
      q1 = this.#all[MathFloor(size / 4)];
      q3 = this.#all[MathFloor(size * 3 / 4)];
    }

    const iqr = q3 - q1;
    const minValue = q1 - iqr * 1.5;
    const maxValue = q3 + iqr * 1.5;

    this.#all = ArrayPrototypeFilter(
      this.#all,
      (value) => (value <= maxValue) && (value >= minValue),
    );
  }

  #calculateMinMax() {
    this.#min = Infinity;
    this.#max = -Infinity;

    for (let i = 0; i < this.#all.length; i++) {
      this.#min = MathMin(this.#all[i], this.#min);
      this.#max = MathMax(this.#all[i], this.#max);
    }
  }

  #calculateMean() {
    if (this.#all.length === 0) {
      this.#mean = 0;
      return;
    }

    if (this.#all.length === 1) {
      this.#mean = this.#all[0];
      return;
    }

    this.#mean = ArrayPrototypeReduce(
      this.#all,
      (acc, value) => MathMin(NumberMAX_SAFE_INTEGER, acc + value),
      0,
    ) / this.#all.length;
  }

  /**
   * References:
   * - https://en.wikipedia.org/wiki/Coefficient_of_variation
   * - https://github.com/google/benchmark/blob/159eb2d0ffb85b86e00ec1f983d72e72009ec387/src/statistics.cc#L81-L88
   */
  #calculateCv() {
    if (this.#all.length < 2) {
      this.#cv = 0;
      return;
    }

    const avgSquares = ArrayPrototypeReduce(
      this.#all,
      (acc, value) => MathMin(NumberMAX_SAFE_INTEGER, MathPow(value, 2) + acc), 0,
    ) * (1 / this.#all.length);

    const stddev = MathSqrt(MathMax(0, this.#all.length / (this.#all.length - 1) * (avgSquares - (this.#mean * this.#mean))));

    this.#cv = stddev / this.#mean;
  }
}

module.exports = {
  StatisticalHistogram,
  kStatisticalHistogramRecord,
  kStatisticalHistogramFinish,
};
