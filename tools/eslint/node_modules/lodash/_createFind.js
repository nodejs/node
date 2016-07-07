var baseIteratee = require('./_baseIteratee'),
    isArrayLike = require('./isArrayLike'),
    keys = require('./keys');

/**
 * Creates a `_.find` or `_.findLast` function.
 *
 * @private
 * @param {Function} findIndexFunc The function to find the collection index.
 * @returns {Function} Returns the new find function.
 */
function createFind(findIndexFunc) {
  return function(collection, predicate, fromIndex) {
    var iterable = Object(collection);
    predicate = baseIteratee(predicate, 3);
    if (!isArrayLike(collection)) {
      var props = keys(collection);
    }
    var index = findIndexFunc(props || collection, function(value, key) {
      if (props) {
        key = value;
        value = iterable[key];
      }
      return predicate(value, key, iterable);
    }, fromIndex);
    return index > -1 ? collection[props ? props[index] : index] : undefined;
  };
}

module.exports = createFind;
