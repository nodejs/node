var baseCastFunction = require('./_baseCastFunction'),
    baseForOwn = require('./_baseForOwn');

/**
 * Iterates over own enumerable properties of an object invoking `iteratee`
 * for each property. The iteratee is invoked with three arguments:
 * (value, key, object). Iteratee functions may exit iteration early by
 * explicitly returning `false`.
 *
 * @static
 * @memberOf _
 * @category Object
 * @param {Object} object The object to iterate over.
 * @param {Function} [iteratee=_.identity] The function invoked per iteration.
 * @returns {Object} Returns `object`.
 * @example
 *
 * function Foo() {
 *   this.a = 1;
 *   this.b = 2;
 * }
 *
 * Foo.prototype.c = 3;
 *
 * _.forOwn(new Foo, function(value, key) {
 *   console.log(key);
 * });
 * // => logs 'a' then 'b' (iteration order is not guaranteed)
 */
function forOwn(object, iteratee) {
  return object && baseForOwn(object, baseCastFunction(iteratee));
}

module.exports = forOwn;
