'use strict';
require('../../../common');
const test = require('node:test');

function funcWithIgnoredBranch(value) {
  /* node:coverage ignore next */
  if (value > 0) { // This branch should be ignored
    return 'positive';
  } else {
    return 'negative';
  }
}

test('should not report BRDA for ignored branch', () => {
  funcWithIgnoredBranch(1); // Only call one path of the branch
});
