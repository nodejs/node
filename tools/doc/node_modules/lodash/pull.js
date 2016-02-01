var pullAll = require('./pullAll'),
    rest = require('./rest');

/**
 * Removes all provided values from `array` using
 * [`SameValueZero`](http://ecma-international.org/ecma-262/6.0/#sec-samevaluezero)
 * for equality comparisons.
 *
 * **Note:** Unlike `_.without`, this method mutates `array`.
 *
 * @static
 * @memberOf _
 * @category Array
 * @param {Array} array The array to modify.
 * @param {...*} [values] The values to remove.
 * @returns {Array} Returns `array`.
 * @example
 *
 * var array = [1, 2, 3, 1, 2, 3];
 *
 * _.pull(array, 2, 3);
 * console.log(array);
 * // => [1, 1]
 */
var pull = rest(pullAll);

module.exports = pull;
