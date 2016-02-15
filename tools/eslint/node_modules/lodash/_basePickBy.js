var baseForIn = require('./_baseForIn');

/**
 * The base implementation of  `_.pickBy` without support for iteratee shorthands.
 *
 * @private
 * @param {Object} object The source object.
 * @param {Function} predicate The function invoked per property.
 * @returns {Object} Returns the new object.
 */
function basePickBy(object, predicate) {
  var result = {};
  baseForIn(object, function(value, key) {
    if (predicate(value, key)) {
      result[key] = value;
    }
  });
  return result;
}

module.exports = basePickBy;
