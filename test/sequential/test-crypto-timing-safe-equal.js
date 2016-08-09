// Flags: --allow_natives_syntax
'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const crypto = require('crypto');

assert.strictEqual(
  crypto.timingSafeEqual(Buffer.from('foo'), Buffer.from('foo')),
  true,
  'should consider equal strings to be equal'
);

assert.strictEqual(
  crypto.timingSafeEqual(Buffer.from('foo'), Buffer.from('bar')),
  false,
  'should consider unequal strings to be unequal'
);

assert.throws(function() {
  crypto.timingSafeEqual(Buffer.from([1, 2, 3]), Buffer.from([1, 2]));
}, 'should throw when given buffers with different lengths');

assert.throws(function() {
  crypto.timingSafeEqual('not a buffer', Buffer.from([1, 2]));
}, 'should throw if the first argument is not a buffer');

assert.throws(function() {
  crypto.timingSafeEqual(Buffer.from([1, 2]), 'not a buffer');
}, 'should throw if the second argument is not a buffer');

function runBenchmark(compareFunc, bufferA, bufferB, expectedResult) {
  const startTime = process.hrtime();
  const result = compareFunc(bufferA, bufferB);
  const endTime = process.hrtime(startTime);

  // Ensure that the result of the function call gets used, so that it doesn't
  // get discarded due to engine optimizations.
  assert.strictEqual(result, expectedResult);
  return endTime[0] * 1e9 + endTime[1];
}

function getTValue(compareFunc) {
  const numTrials = 10000;
  const testBufferSize = 10000;
  // Perform benchmarks to verify that timingSafeEqual is actually timing-safe.
  const bufferA1 = Buffer.alloc(testBufferSize, 'A');
  const bufferA2 = Buffer.alloc(testBufferSize, 'A');
  const bufferB = Buffer.alloc(testBufferSize, 'B');
  const bufferC = Buffer.alloc(testBufferSize, 'C');

  const rawEqualBenches = Array(numTrials);
  const rawUnequalBenches = Array(numTrials);

  for (let i = 0; i < numTrials; i++) {
    // First benchmark: comparing two equal buffers
    rawEqualBenches[i] = runBenchmark(compareFunc, bufferA1, bufferA2, true);
    // Second benchmark: comparing two unequal buffers
    rawUnequalBenches[i] = runBenchmark(compareFunc, bufferB, bufferC, false);
  }

  const equalBenches = filterOutliers(rawEqualBenches);
  const unequalBenches = filterOutliers(rawUnequalBenches);

  // Use a two-sample t-test to determine whether the timing difference between
  // the benchmarks is statistically significant.
  // https://wikipedia.org/wiki/Student%27s_t-test#Independent_two-sample_t-test

  const equalMean = mean(equalBenches);
  const unequalMean = mean(unequalBenches);

  const equalLen = equalBenches.length;
  const unequalLen = unequalBenches.length;

  const combinedStd = combinedStandardDeviation(equalBenches, unequalBenches);
  const standardErr = combinedStd * Math.sqrt(1 / equalLen + 1 / unequalLen);

  return (equalMean - unequalMean) / standardErr;
}

// Returns the mean of an array
function mean(array) {
  return array.reduce((sum, val) => sum + val, 0) / array.length;
}

// Returns the sample standard deviation of an array
function standardDeviation(array) {
  const arrMean = mean(array);
  const total = array.reduce((sum, val) => sum + Math.pow(val - arrMean, 2), 0);
  return Math.sqrt(total / (array.length - 1));
}

// Returns the common standard deviation of two arrays
function combinedStandardDeviation(array1, array2) {
  const sum1 = Math.pow(standardDeviation(array1), 2) * (array1.length - 1);
  const sum2 = Math.pow(standardDeviation(array2), 2) * (array2.length - 1);
  return Math.sqrt((sum1 + sum2) / (array1.length + array2.length - 2));
}

// Filter large outliers from an array. A 'large outlier' is a value that is at
// least 50 times larger than the mean. This prevents the tests from failing
// due to the standard deviation increase when a function unexpectedly takes
// a very long time to execute.
function filterOutliers(array) {
  const arrMean = mean(array);
  return array.filter((value) => value / arrMean < 50);
}

// Force optimization before starting the benchmark
runBenchmark(crypto.timingSafeEqual, Buffer.from('A'), Buffer.from('A'), true);
eval('%OptimizeFunctionOnNextCall(runBenchmark)');
runBenchmark(crypto.timingSafeEqual, Buffer.from('A'), Buffer.from('A'), true);

// t_(0.9995, ∞)
// i.e. If a given comparison function is indeed timing-safe, the t-test result
// has a 99.9% chance to be below this threshold. Unfortunately, this means that
// this test will be a bit flakey and will fail 0.1% of the time even if
// crypto.timingSafeEqual is working properly.
// t-table ref: http://www.sjsu.edu/faculty/gerstman/StatPrimer/t-table.pdf
// Note that in reality there are roughly `2 * numTrials - 2` degrees of
// freedom, not ∞. However, assuming `numTrials` is large, this doesn't
// significantly affect the threshold.
const T_THRESHOLD = 3.291;

const t = getTValue(crypto.timingSafeEqual);
assert(
  Math.abs(t) < T_THRESHOLD,
  `timingSafeEqual should not leak information from its execution time (t=${t})`
);

// As a sanity check to make sure the statistical tests are working, run the
// same benchmarks again, this time with an unsafe comparison function. In this
// case the t-value should be above the threshold.
const unsafeCompare = (bufA, bufB) => bufA.equals(bufB);
const t2 = getTValue(unsafeCompare);
assert(
  Math.abs(t2) > T_THRESHOLD,
  `Buffer#equals should leak information from its execution time (t=${t2})`
);
