/**
 * Subtract two numbers.
 *
 * @static
 * @memberOf _
 * @category Math
 * @param {number} minuend The first number in a subtraction.
 * @param {number} subtrahend The second number in a subtraction.
 * @returns {number} Returns the difference.
 * @example
 *
 * _.subtract(6, 4);
 * // => 2
 */
function subtract(minuend, subtrahend) {
  var result;
  if (minuend !== undefined) {
    result = minuend;
  }
  if (subtrahend !== undefined) {
    result = result === undefined ? subtrahend : (result - subtrahend);
  }
  return result;
}

module.exports = subtract;
