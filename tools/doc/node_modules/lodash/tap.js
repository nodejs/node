/**
 * This method invokes `interceptor` and returns `value`. The interceptor is
 * invoked with one argument; (value). The purpose of this method is to "tap into"
 * a method chain in order to perform operations on intermediate results within
 * the chain.
 *
 * @static
 * @memberOf _
 * @category Seq
 * @param {*} value The value to provide to `interceptor`.
 * @param {Function} interceptor The function to invoke.
 * @returns {*} Returns `value`.
 * @example
 *
 * _([1, 2, 3])
 *  .tap(function(array) {
 *    array.pop();
 *  })
 *  .reverse()
 *  .value();
 * // => [2, 1]
 */
function tap(value, interceptor) {
  interceptor(value);
  return value;
}

module.exports = tap;
