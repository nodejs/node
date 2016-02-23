var baseFor = require('./_baseFor'),
    keysIn = require('./keysIn'),
    toFunction = require('./_toFunction');

/**
 * Iterates over own and inherited enumerable properties of an object invoking
 * `iteratee` for each property. The iteratee is invoked with three arguments:
 * (value, key, object). Iteratee functions may exit iteration early by explicitly
 * returning `false`.
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
 * _.forIn(new Foo, function(value, key) {
 *   console.log(key);
 * });
 * // => logs 'a', 'b', then 'c' (iteration order is not guaranteed)
 */
function forIn(object, iteratee) {
  return object == null ? object : baseFor(object, toFunction(iteratee), keysIn);
}

module.exports = forIn;
