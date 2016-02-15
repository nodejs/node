var baseIteratee = require('./_baseIteratee'),
    basePickBy = require('./_basePickBy');

/**
 * The opposite of `_.pickBy`; this method creates an object composed of the
 * own and inherited enumerable properties of `object` that `predicate`
 * doesn't return truthy for.
 *
 * @static
 * @memberOf _
 * @category Object
 * @param {Object} object The source object.
 * @param {Function|Object|string} [predicate=_.identity] The function invoked per property.
 * @returns {Object} Returns the new object.
 * @example
 *
 * var object = { 'a': 1, 'b': '2', 'c': 3 };
 *
 * _.omitBy(object, _.isNumber);
 * // => { 'b': '2' }
 */
function omitBy(object, predicate) {
  predicate = baseIteratee(predicate, 2);
  return basePickBy(object, function(value, key) {
    return !predicate(value, key);
  });
}

module.exports = omitBy;
