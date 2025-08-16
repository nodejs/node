'use strict';

// This test validates the --max-old-space-size-percentage flag functionality

require('../common');
const assert = require('node:assert');
const { spawnSync } = require('child_process');
const os = require('os');

// Valid cases
const validPercentages = [
  '1', '10', '25', '50', '75', '99', '100', '25.5',
];

// Invalid cases
const invalidPercentages = [
  ['', /--max-old-space-size-percentage= requires an argument/],
  ['0', /--max-old-space-size-percentage must be greater than 0 and up to 100\. Got: 0/],
  ['101', /--max-old-space-size-percentage must be greater than 0 and up to 100\. Got: 101/],
  ['-1', /--max-old-space-size-percentage must be greater than 0 and up to 100\. Got: -1/],
  ['abc', /--max-old-space-size-percentage must be greater than 0 and up to 100\. Got: abc/],
  ['1%', /--max-old-space-size-percentage must be greater than 0 and up to 100\. Got: 1%/],
];

// Test valid cases
validPercentages.forEach((input) => {
  const result = spawnSync(process.execPath, [
    `--max-old-space-size-percentage=${input}`,
  ], { stdio: ['pipe', 'pipe', 'pipe'] });
  assert.strictEqual(result.status, 0, `Expected exit code 0 for valid input ${input}`);
  assert.strictEqual(result.stderr.toString(), '', `Expected empty stderr for valid input ${input}`);
});

// Test invalid cases
invalidPercentages.forEach((input) => {
  const result = spawnSync(process.execPath, [
    `--max-old-space-size-percentage=${input[0]}`,
  ], { stdio: ['pipe', 'pipe', 'pipe'] });
  assert.notStrictEqual(result.status, 0, `Expected non-zero exit for invalid input ${input[0]}`);
  assert(input[1].test(result.stderr.toString()), `Unexpected error message for invalid input ${input[0]}`);
});

// Test NODE_OPTIONS with valid percentages
validPercentages.forEach((input) => {
  const result = spawnSync(process.execPath, [], {
    stdio: ['pipe', 'pipe', 'pipe'],
    env: { ...process.env, NODE_OPTIONS: `--max-old-space-size-percentage=${input}` }
  });
  assert.strictEqual(result.status, 0, `NODE_OPTIONS: Expected exit code 0 for valid input ${input}`);
  assert.strictEqual(result.stderr.toString(), '', `NODE_OPTIONS: Expected empty stderr for valid input ${input}`);
});

// Test NODE_OPTIONS with invalid percentages
invalidPercentages.forEach((input) => {
  const result = spawnSync(process.execPath, [], {
    stdio: ['pipe', 'pipe', 'pipe'],
    env: { ...process.env, NODE_OPTIONS: `--max-old-space-size-percentage=${input[0]}` }
  });
  assert.notStrictEqual(result.status, 0, `NODE_OPTIONS: Expected non-zero exit for invalid input ${input[0]}`);
  assert(input[1].test(result.stderr.toString()), `NODE_OPTIONS: Unexpected error message for invalid input ${input[0]}`);
});

// Test percentage calculation validation
function getHeapSizeForPercentage(percentage) {
  const result = spawnSync(process.execPath, [
    '--max-old-space-size=3000', // This value should be ignored, since percentage takes precedence
    `--max-old-space-size-percentage=${percentage}`,
    '--max-old-space-size=1000', // This value should be ignored, since percentage take precedence
    '-e', `
      const v8 = require('v8');
      const stats = v8.getHeapStatistics();
      const heapSizeLimitMB = Math.floor(stats.heap_size_limit / 1024 / 1024);
      console.log(heapSizeLimitMB);
    `,
  ], {
    stdio: ['pipe', 'pipe', 'pipe'],
    env: {
      ...process.env,
      NODE_OPTIONS: `--max-old-space-size=2000` // This value should be ignored, since percentage takes precedence
    }
  });

  if (result.status !== 0) {
    throw new Error(`Failed to get heap size for ${percentage}: ${result.stderr.toString()}`);
  }

  return parseInt(result.stdout.toString(), 10);
}

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
  ratio25to100 >= 0.15 && ratio25to100 <= 0.35,
  `25% heap size should be roughly quarter of 100% (got ${ratio25to100.toFixed(2)}, expected ~0.25)`
);

// 75% should be roughly three-quarters of 100%
const ratio75to100 = heapSizes[75] / heapSizes[100];
assert(
  ratio75to100 >= 0.65 && ratio75to100 <= 0.85,
  `75% heap size should be roughly three-quarters of 100% (got ${ratio75to100.toFixed(2)}, expected ~0.75)`
);

// Validate heap sizes against system memory
const totalMemoryMB = Math.floor(os.totalmem() / 1024 / 1024);
const uint64Max = 2 ** 64 - 1;
const constrainedMemory = process.constrainedMemory();
const constrainedMemoryMB = Math.floor(constrainedMemory / 1024 / 1024);
const effectiveMemoryMB =
  constrainedMemory > 0 && constrainedMemory !== uint64Max ? constrainedMemoryMB : totalMemoryMB;
const margin = 10; // 10% margin
testPercentages.forEach((percentage) => {
  const upperLimit = effectiveMemoryMB * ((percentage + margin) / 100);
  assert(
    heapSizes[percentage] <= upperLimit,
    `Heap size for ${percentage}% (${heapSizes[percentage]} MB) should not exceed upper limit (${upperLimit} MB)`
  );
  const lowerLimit = effectiveMemoryMB * ((percentage - margin) / 100);
  assert(
    heapSizes[percentage] >= lowerLimit,
    `Heap size for ${percentage}% (${heapSizes[percentage]} MB) should not be less than lower limit (${lowerLimit} MB)`
  );
});
