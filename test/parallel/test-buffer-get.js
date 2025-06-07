'use strict';

require('../common');

const assert = require('assert');

const BASE_BUFFER = Buffer.from([0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78]);

// Sequential-read test cases (multiple reads with advancing positions)
const sequentialTestCases = [
  {
    steps: [
      {
        method: 'getIntBE',
        byteLength: 2,
        expected: 0x1234,
      },
      {
        method: 'getIntBE',
        byteLength: 2,
        expected: 0x5678,
      },
    ],
  },
  {
    steps: [
      {
        method: 'getIntLE',
        byteLength: 2,
        expected: 0x3412,
      },
      {
        method: 'getIntLE',
        byteLength: 2,
        expected: 0x7856,
      },
    ],
  },
  {
    steps: [
      {
        method: 'getUIntBE',
        byteLength: 2,
        expected: 0x1234,
      },
      {
        method: 'getUIntBE',
        byteLength: 2,
        expected: 0x5678,
      },
    ],
  },
  {
    steps: [
      {
        method: 'getUIntLE',
        byteLength: 2,
        expected: 0x3412,
      },
      {
        method: 'getUIntLE',
        byteLength: 2,
        expected: 0x7856,
      },
    ],
  },
];

// Mixed-endian test cases (mixing big-endian and little-endian reads)
const mixedEndianTestCases = [
  {
    steps: [
      {
        method: 'getIntBE',
        byteLength: 2,
        expected: 0x1234,
      },
      {
        method: 'getIntLE',
        byteLength: 2,
        expected: 0x7856,
      },
    ],
  },
  {
    steps: [
      {
        method: 'getUIntBE',
        byteLength: 2,
        expected: 0x1234,
      },
      {
        method: 'getUIntLE',
        byteLength: 2,
        expected: 0x7856,
      },
    ],
  },
];

function runTestCase({ steps }, index) {
  // Clone the buffer to avoid side effects
  const buffer = Buffer.from(BASE_BUFFER);

  steps.forEach(({ method, byteLength, expected }) => {
    assert.strictEqual(
      buffer[method](byteLength),
      expected,
      `Method ${method} at step ${index + 1} failed`
    );
  });
}

function runTestCases(testCases) {
  testCases.forEach((testCase, index) => runTestCase(testCase, index));
}

runTestCases(sequentialTestCases);
runTestCases(mixedEndianTestCases);

// Out-of-bounds test cases
{
  const buffer = Buffer.from([0x01, 0x02]);
  const byteLength = 2;

  buffer.getIntBE(1); // Moves reader index to 1

  const methodsToTest = ['getIntBE', 'getIntLE', 'getUIntBE', 'getUIntLE'];

  methodsToTest.forEach((method) => {
    assert.throws(() => buffer[method](byteLength), { name: 'RangeError' });
  });

  // Check that the reader index is not moved
  assert.strictEqual(buffer.readerIndex, 1);
  assert.strictEqual(buffer.getIntBE(1), 0x02);
}
