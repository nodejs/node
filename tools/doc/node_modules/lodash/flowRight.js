var createFlow = require('./internal/createFlow');

/**
 * This method is like `_.flow` except that it creates a function that
 * invokes the provided functions from right to left.
 *
 * @static
 * @memberOf _
 * @category Util
 * @param {...(Function|Function[])} [funcs] Functions to invoke.
 * @returns {Function} Returns the new function.
 * @example
 *
 * function square(n) {
 *   return n * n;
 * }
 *
 * var addSquare = _.flowRight(square, _.add);
 * addSquare(1, 2);
 * // => 9
 */
var flowRight = createFlow(true);

module.exports = flowRight;
