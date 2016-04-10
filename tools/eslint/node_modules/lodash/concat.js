var arrayConcat = require('./_arrayConcat'),
    baseFlatten = require('./_baseFlatten'),
    castArray = require('./castArray'),
    copyArray = require('./_copyArray');

/**
 * Creates a new array concatenating `array` with any additional arrays
 * and/or values.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Array
 * @param {Array} array The array to concatenate.
 * @param {...*} [values] The values to concatenate.
 * @returns {Array} Returns the new concatenated array.
 * @example
 *
 * var array = [1];
 * var other = _.concat(array, 2, [3], [[4]]);
 *
 * console.log(other);
 * // => [1, 2, 3, [4]]
 *
 * console.log(array);
 * // => [1]
 */
function concat() {
  var length = arguments.length,
      array = castArray(arguments[0]);

  if (length < 2) {
    return length ? copyArray(array) : [];
  }
  var args = Array(length - 1);
  while (length--) {
    args[length - 1] = arguments[length];
  }
  return arrayConcat(array, baseFlatten(args, 1));
}

module.exports = concat;
