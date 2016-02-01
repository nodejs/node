var baseRange = require('./baseRange'),
    isIterateeCall = require('./isIterateeCall'),
    toNumber = require('../toNumber');

/**
 * Creates a `_.range` or `_.rangeRight` function.
 *
 * @private
 * @param {boolean} [fromRight] Specify iterating from right to left.
 * @returns {Function} Returns the new range function.
 */
function createRange(fromRight) {
  return function(start, end, step) {
    if (step && typeof step != 'number' && isIterateeCall(start, end, step)) {
      end = step = undefined;
    }
    // Ensure the sign of `-0` is preserved.
    start = toNumber(start);
    start = start === start ? start : 0;
    if (end === undefined) {
      end = start;
      start = 0;
    } else {
      end = toNumber(end) || 0;
    }
    step = step === undefined ? (start < end ? 1 : -1) : (toNumber(step) || 0);
    return baseRange(start, end, step, fromRight);
  };
}

module.exports = createRange;
