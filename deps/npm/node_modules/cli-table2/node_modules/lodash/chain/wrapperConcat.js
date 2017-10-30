var arrayConcat = require('../internal/arrayConcat'),
    baseFlatten = require('../internal/baseFlatten'),
    isArray = require('../lang/isArray'),
    restParam = require('../function/restParam'),
    toObject = require('../internal/toObject');

/**
 * Creates a new array joining a wrapped array with any additional arrays
 * and/or values.
 *
 * @name concat
 * @memberOf _
 * @category Chain
 * @param {...*} [values] The values to concatenate.
 * @returns {Array} Returns the new concatenated array.
 * @example
 *
 * var array = [1];
 * var wrapped = _(array).concat(2, [3], [[4]]);
 *
 * console.log(wrapped.value());
 * // => [1, 2, 3, [4]]
 *
 * console.log(array);
 * // => [1]
 */
var wrapperConcat = restParam(function(values) {
  values = baseFlatten(values);
  return this.thru(function(array) {
    return arrayConcat(isArray(array) ? array : [toObject(array)], values);
  });
});

module.exports = wrapperConcat;
