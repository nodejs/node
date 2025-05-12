'use strict';

require('../common');
const assert = require('assert');

const buffer1 = Buffer.from([
  0x12, 0x34, 0x56, 0x78,
  0x12, 0x34, 0x56, 0x78,
  0x12, 0x34, 0x56, 0x78,
  0x12, 0x34, 0x56, 0x78,
]);

// Sequential-read test cases (multiple reads with advancing positions)
const testCases = [
  // 8-bit signed integers
  {
    steps: [
      { method: 'getInt8', expected: 0x12 },
      { method: 'getInt8', expected: 0x34 },
    ],
  },
  // 8-bit unsigned integers
  {
    steps: [
      { method: 'getUInt8', expected: 0x12 },
      { method: 'getUInt8', expected: 0x34 },
    ],
  },
  // 16-bit signed integers
  {
    steps: [
      { method: 'getInt16BE', expected: 0x1234 },
      { method: 'getInt16BE', expected: 0x5678 },
    ],
  },
  {
    steps: [
      { method: 'getInt16LE', expected: 0x3412 },
      { method: 'getInt16LE', expected: 0x7856 },
    ],
  },
  // 16-bit unsigned integers
  {
    steps: [
      { method: 'getUInt16BE', expected: 0x1234 },
      { method: 'getUInt16BE', expected: 0x5678 },
    ],
  },
  {
    steps: [
      { method: 'getUInt16LE', expected: 0x3412 },
      { method: 'getUInt16LE', expected: 0x7856 },
    ],
  },
  // 32-bit signed integers
  {
    steps: [
      { method: 'getInt32BE', expected: 0x12345678 },
      { method: 'getInt32BE', expected: 0x12345678 },
    ],
  },
  {
    steps: [
      { method: 'getInt32LE', expected: 0x78563412 },
      { method: 'getInt32LE', expected: 0x78563412 },
    ],
  },
  // 32-bit unsigned integers
  {
    steps: [
      { method: 'getUInt32BE', expected: 0x12345678 },
      { method: 'getUInt32BE', expected: 0x12345678 },
    ],
  },
  {
    steps: [
      { method: 'getUInt32LE', expected: 0x78563412 },
      { method: 'getUInt32LE', expected: 0x78563412 },
    ],
  },
  // 64-bit signed integers
  {
    steps: [
      { method: 'getBigInt64BE', expected: BigInt('0x1234567812345678') },
      { method: 'getBigInt64BE', expected: BigInt('0x1234567812345678') },
    ],
  },
  {
    steps: [
      { method: 'getBigInt64LE', expected: BigInt('0x7856341278563412') },
      { method: 'getBigInt64LE', expected: BigInt('0x7856341278563412') },
    ],
  },
  // 64-bit unsigned integers
  {
    steps: [
      { method: 'getBigUInt64BE', expected: BigInt('0x1234567812345678') },
      { method: 'getBigUInt64BE', expected: BigInt('0x1234567812345678') },
    ],
  },
  {
    steps: [
      { method: 'getBigUInt64LE', expected: BigInt('0x7856341278563412') },
      { method: 'getBigUInt64LE', expected: BigInt('0x7856341278563412') },
    ],
  },
];

function runTestCase({ steps }, index) {
  // Clone the buffer to avoid side effects
  const buffer = Buffer.from(buffer1);

  steps.forEach(({ method, expected }) => {
    assert.strictEqual(
      buffer[method](),
      expected,
      `Method ${method} at step ${index + 1} failed`
    );
  });
}

function runTestCases(testCases) {
  testCases.forEach((testCase, index) => runTestCase(testCase, index));
}

// Out-of-bounds test cases using a empty buffer
{
  const emptyBuffer = Buffer.from([]);

  const methodsToTest = [
    'getInt8', 'getUInt8',
    'getInt16BE', 'getInt16LE',
    'getUInt16BE', 'getUInt16LE',
    'getInt32BE', 'getInt32LE',
    'getUInt32BE', 'getUInt32LE',
    'getBigInt64BE', 'getBigInt64LE',
    'getBigUInt64BE', 'getBigUInt64LE',
  ];

  methodsToTest.forEach((method) => {
    assert.throws(() => emptyBuffer[method](), {
      code: 'ERR_BUFFER_OUT_OF_BOUNDS',
      name: 'RangeError',
      message: 'Attempt to access memory outside buffer bounds',
    });
  });

  // Check that the reader index is not moved
  assert.strictEqual(emptyBuffer.readerIndex, 0);
}

runTestCases(testCases);
