var baseCastFunction = require('./_baseCastFunction'),
    baseForRight = require('./_baseForRight'),
    keysIn = require('./keysIn');

/**
 * This method is like `_.forIn` except that it iterates over properties of
 * `object` in the opposite order.
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
 * _.forInRight(new Foo, function(value, key) {
 *   console.log(key);
 * });
 * // => logs 'c', 'b', then 'a' assuming `_.forIn` logs 'a', 'b', then 'c'
 */
function forInRight(object, iteratee) {
  return object == null
    ? object
    : baseForRight(object, baseCastFunction(iteratee), keysIn);
}

module.exports = forInRight;
