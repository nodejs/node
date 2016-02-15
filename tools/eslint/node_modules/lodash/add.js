/**
 * Adds two numbers.
 *
 * @static
 * @memberOf _
 * @category Math
 * @param {number} augend The first number in an addition.
 * @param {number} addend The second number in an addition.
 * @returns {number} Returns the total.
 * @example
 *
 * _.add(6, 4);
 * // => 10
 */
function add(augend, addend) {
  var result;
  if (augend === undefined && addend === undefined) {
    return 0;
  }
  if (augend !== undefined) {
    result = augend;
  }
  if (addend !== undefined) {
    result = result === undefined ? addend : (result + addend);
  }
  return result;
}

module.exports = add;
