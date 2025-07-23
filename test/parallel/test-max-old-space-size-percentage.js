'use strict';

// This test validates the --max-old-space-size=XX% CLI flag functionality.
// It tests valid and invalid percentage values, NODE_OPTIONS integration,
// backward compatibility with MB values, and percentage calculation accuracy.

require('../common');
const assert = require('node:assert');
const { spawnSync } = require('child_process');

// Valid percentage cases
const validPercentages = [
  '1%', '10%', '25%', '50%', '75%', '99%', '100%', '25.5%',
];

// Invalid percentage cases
const invalidPercentages = [
  '%', '0%', '101%', '-1%', 'abc%', '100.1%', '0.0%',
];

// Helper for error message matching
function assertErrorMessage(stderr, context) {
  assert(
    /illegal value for flag --max-old-space-size=|--max-old-space-size percentage must be|--max-old-space-size percentage must not be empty/.test(stderr),
    `Expected error message for ${context}, got: ${stderr}`
  );
}

// Test valid percentage cases
validPercentages.forEach((input) => {
  const result = spawnSync(process.execPath, [
    `--max-old-space-size=${input}`,
    '-e', 'console.log("OK")',
  ], { stdio: ['pipe', 'pipe', 'pipe'] });
  assert.strictEqual(result.status, 0, `Expected exit code 0 for valid input ${input}`);
  assert.match(result.stdout.toString(), /OK/, `Expected stdout to contain OK for valid input ${input}`);
  assert.strictEqual(result.stderr.toString(), '', `Expected empty stderr for valid input ${input}`);
});

// Test invalid percentage cases
invalidPercentages.forEach((input) => {
  const result = spawnSync(process.execPath, [
    `--max-old-space-size=${input}`,
    '-e', 'console.log("FAIL")',
  ], { stdio: ['pipe', 'pipe', 'pipe'] });
  assert.notStrictEqual(result.status, 0, `Expected non-zero exit for invalid input ${input}`);
  assertErrorMessage(result.stderr.toString(), input);
});

// Test NODE_OPTIONS with valid percentages
validPercentages.forEach((input) => {
  const result = spawnSync(process.execPath, [
    '-e', 'console.log("NODE_OPTIONS OK")',
  ], {
    stdio: ['pipe', 'pipe', 'pipe'],
    env: { ...process.env, NODE_OPTIONS: `--max-old-space-size=${input}` }
  });
  assert.strictEqual(result.status, 0, `NODE_OPTIONS: Expected exit code 0 for valid input ${input}`);
  assert.strictEqual(result.stderr.toString(), '', `NODE_OPTIONS: Expected empty stderr for valid input ${input}`);
  assert.match(result.stdout.toString(), /NODE_OPTIONS OK/, `NODE_OPTIONS: Expected stdout for valid input ${input}`);
});

// Test NODE_OPTIONS with invalid percentages
invalidPercentages.forEach((input) => {
  const result = spawnSync(process.execPath, [
    '-e', 'console.log("NODE_OPTIONS FAIL")',
  ], {
    stdio: ['pipe', 'pipe', 'pipe'],
    env: { ...process.env, NODE_OPTIONS: `--max-old-space-size=${input}` }
  });
  assert.notStrictEqual(result.status, 0, `NODE_OPTIONS: Expected non-zero exit for invalid input ${input}`);
  assertErrorMessage(result.stderr.toString(), `NODE_OPTIONS ${input}`);
});

// Test backward compatibility: MB values
const maxOldSpaceSizeMB = 600; // Example MB value
const mbResult = spawnSync(process.execPath, [
  `--max-old-space-size=${maxOldSpaceSizeMB}`,
  '-e', 'console.log("Regular MB test")',
], { stdio: ['pipe', 'pipe', 'pipe'] });
assert.strictEqual(mbResult.status, 0, `Expected exit code 0 for MB value ${maxOldSpaceSizeMB}`);
assert.match(mbResult.stdout.toString(), /Regular MB test/, `Expected stdout for MB value but received ${mbResult.stdout.toString()} for value ${maxOldSpaceSizeMB}`);
assert.strictEqual(mbResult.stderr.toString(), '', `Expected empty stderr for MB value ${maxOldSpaceSizeMB}`);

// Test percentage calculation validation
function getHeapSizeForPercentage(percentage) {
  const result = spawnSync(process.execPath, [
    `--max-old-space-size=${percentage}%`,
    '-e', `
      const v8 = require('v8');
      const stats = v8.getHeapStatistics();
      const heapSizeLimitMB = Math.floor(stats.heap_size_limit / 1024 / 1024);
      console.log(heapSizeLimitMB);
    `,
  ], { stdio: ['pipe', 'pipe', 'pipe'] });

  if (result.status !== 0) {
    throw new Error(`Failed to get heap size for ${percentage}%: ${result.stderr.toString()}`);
  }

  return parseInt(result.stdout.toString(), 10);
}

// Test that percentages produce reasonable heap sizes
const testPercentages = [25, 50, 75, 100];
const heapSizes = {};

// Get heap sizes for all test percentages
testPercentages.forEach((percentage) => {
  heapSizes[percentage] = getHeapSizeForPercentage(percentage);
});

// Test relative relationships between percentages
// 50% should be roughly half of 100%
const ratio50to100 = heapSizes[50] / heapSizes[100];
assert(
  ratio50to100 >= 0.4 && ratio50to100 <= 0.6,
  `50% heap size should be roughly half of 100% (got ${ratio50to100.toFixed(2)}, expected ~0.5)`
);

// 25% should be roughly quarter of 100%
const ratio25to100 = heapSizes[25] / heapSizes[100];
assert(
  ratio25to100 >= 0.2 && ratio25to100 <= 0.4,
  `25% heap size should be roughly quarter of 100% (got ${ratio25to100.toFixed(2)}, expected ~0.25)`
);

// 75% should be roughly three-quarters of 100%
const ratio75to100 = heapSizes[75] / heapSizes[100];
assert(
  ratio75to100 >= 0.6 && ratio75to100 <= 0.9,
  `75% heap size should be roughly three-quarters of 100% (got ${ratio75to100.toFixed(2)}, expected ~0.75)`
);

// Test that larger percentages produce larger heap sizes
assert(heapSizes[25] <= heapSizes[50], '25% should produce smaller heap than 50%');
assert(heapSizes[50] <= heapSizes[75], '50% should produce smaller heap than 75%');
assert(heapSizes[75] <= heapSizes[100], '75% should produce smaller heap than 100%');
