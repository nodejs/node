'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeReverse,
} = primordials;

const { validateStringArray, validateString } = require('internal/validators');
const { myersDiff } = require('internal/assert/myers_diff');

function validateInput(value, name) {
  if (!ArrayIsArray(value)) {
    validateString(value, name);
    return;
  }

  validateStringArray(value, name);
}

/**
 * Generate a difference report between two values
 * @param {Array | string} actual - The first value to compare
 * @param {Array | string} expected - The second value to compare
 * @returns {Array} - An array of differences between the two values.
 * The returned data is an array of arrays, where each sub-array has two elements:
 * 1. The operation to perform: -1 for delete, 0 for no-op, 1 for insert
 * 2. The value to perform the operation on
 */
function diff(actual, expected) {
  if (actual === expected) {
    return [];
  }

  validateInput(actual, 'actual');
  validateInput(expected, 'expected');

  return ArrayPrototypeReverse(myersDiff(actual, expected));
}

module.exports = {
  diff,
};
