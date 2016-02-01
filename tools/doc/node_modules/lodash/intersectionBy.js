var arrayMap = require('./internal/arrayMap'),
    baseIntersection = require('./internal/baseIntersection'),
    baseIteratee = require('./internal/baseIteratee'),
    last = require('./last'),
    rest = require('./rest'),
    toArrayLikeObject = require('./internal/toArrayLikeObject');

/**
 * This method is like `_.intersection` except that it accepts `iteratee`
 * which is invoked for each element of each `arrays` to generate the criterion
 * by which uniqueness is computed. The iteratee is invoked with one argument: (value).
 *
 * @static
 * @memberOf _
 * @category Array
 * @param {...Array} [arrays] The arrays to inspect.
 * @param {Function|Object|string} [iteratee=_.identity] The iteratee invoked per element.
 * @returns {Array} Returns the new array of shared values.
 * @example
 *
 * _.intersectionBy([2.1, 1.2], [4.3, 2.4], Math.floor);
 * // => [2.1]
 *
 * // using the `_.property` iteratee shorthand
 * _.intersectionBy([{ 'x': 1 }], [{ 'x': 2 }, { 'x': 1 }], 'x');
 * // => [{ 'x': 1 }]
 */
var intersectionBy = rest(function(arrays) {
  var iteratee = last(arrays),
      mapped = arrayMap(arrays, toArrayLikeObject);

  if (iteratee === last(mapped)) {
    iteratee = undefined;
  } else {
    mapped.pop();
  }
  return (mapped.length && mapped[0] === arrays[0])
    ? baseIntersection(mapped, baseIteratee(iteratee))
    : [];
});

module.exports = intersectionBy;
