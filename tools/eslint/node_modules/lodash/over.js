var arrayMap = require('./_arrayMap'),
    createOver = require('./_createOver');

/**
 * Creates a function that invokes `iteratees` with the arguments provided
 * to the created function and returns their results.
 *
 * @static
 * @memberOf _
 * @category Util
 * @param {...(Function|Function[])} iteratees The iteratees to invoke.
 * @returns {Function} Returns the new function.
 * @example
 *
 * var func = _.over(Math.max, Math.min);
 *
 * func(1, 2, 3, 4);
 * // => [4, 1]
 */
var over = createOver(arrayMap);

module.exports = over;
