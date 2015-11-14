/**
 * Gets the first element of `array`.
 *
 * @static
 * @memberOf _
 * @alias first
 * @category Array
 * @param {Array} array The array to query.
 * @returns {*} Returns the first element of `array`.
 * @example
 *
 * _.head([1, 2, 3]);
 * // => 1
 *
 * _.head([]);
 * // => undefined
 */
function head(array) {
  return array ? array[0] : undefined;
}

module.exports = head;
