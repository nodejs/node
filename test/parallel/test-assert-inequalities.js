'use strict';

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const testCases = {
  greaterThan: [
    { actual: 5, expected: 3, shouldPass: true },
    { actual: 3, expected: 3, shouldPass: false },
    { actual: 2, expected: 3, shouldPass: false },
  ],
  atLeast: [
    { actual: 5, expected: 3, shouldPass: true },
    { actual: 3, expected: 3, shouldPass: true },
    { actual: 2, expected: 3, shouldPass: false },
  ],
  lessThan: [
    { actual: 2, expected: 3, shouldPass: true },
    { actual: 3, expected: 3, shouldPass: false },
    { actual: 4, expected: 3, shouldPass: false },
  ],
  atMost: [
    { actual: 2, expected: 3, shouldPass: true },
    { actual: 3, expected: 3, shouldPass: true },
    { actual: 4, expected: 3, shouldPass: false },
  ]
};

const comparison = {
  greaterThan: 'greater than',
  atLeast: 'greater than or equal to',
  lessThan: 'less than',
  atMost: 'less than or equal to'
};

const methods = Object.keys(testCases);

for (const method of methods) {
  describe(`assert.${method}`, () => {
    for (const { actual, expected, shouldPass } of testCases[method]) {
      it(`should ${shouldPass ? 'pass' : 'fail'} with (${actual}, ${expected})`, () => {
        assert[shouldPass ? 'doesNotThrow' : 'throws'](
          () => assert[method](actual, expected),
          new RegExp(`Expected ${actual} to be ${comparison[method]} ${expected}`)
        );
      });
    }
  });
}
