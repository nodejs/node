var arrayMap = require('./internal/arrayMap'),
    baseIntersection = require('./internal/baseIntersection'),
    rest = require('./rest'),
    toArrayLikeObject = require('./internal/toArrayLikeObject');

/**
 * Creates an array of unique values that are included in all of the provided
 * arrays using [`SameValueZero`](http://ecma-international.org/ecma-262/6.0/#sec-samevaluezero)
 * for equality comparisons.
 *
 * @static
 * @memberOf _
 * @category Array
 * @param {...Array} [arrays] The arrays to inspect.
 * @returns {Array} Returns the new array of shared values.
 * @example
 *
 * _.intersection([2, 1], [4, 2], [1, 2]);
 * // => [2]
 */
var intersection = rest(function(arrays) {
  var mapped = arrayMap(arrays, toArrayLikeObject);
  return (mapped.length && mapped[0] === arrays[0])
    ? baseIntersection(mapped)
    : [];
});

module.exports = intersection;
