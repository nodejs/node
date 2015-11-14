/**
 * This method is the wrapper version of `_.flatMap`.
 *
 * @name flatMap
 * @memberOf _
 * @category Seq
 * @param {Function|Object|string} [iteratee=_.identity] The function invoked per iteration.
 * @returns {Object} Returns the new `lodash` wrapper instance.
 * @example
 *
 * function duplicate(n) {
 *   return [n, n];
 * }
 *
 * _([1, 2]).flatMap(duplicate).value();
 * // => [1, 1, 2, 2]
 */
function wrapperFlatMap(iteratee) {
  return this.map(iteratee).flatten();
}

module.exports = wrapperFlatMap;
