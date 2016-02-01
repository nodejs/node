var arrayReduce = require('./internal/arrayReduce'),
    keys = require('./keys');

/** Used for built-in method references. */
var objectProto = global.Object.prototype;

/** Used to check objects for own properties. */
var hasOwnProperty = objectProto.hasOwnProperty;

/**
 * Creates an object composed of the inverted keys and values of `object`.
 * If `object` contains duplicate values, subsequent values overwrite property
 * assignments of previous values unless `multiVal` is `true`.
 *
 * @static
 * @memberOf _
 * @category Object
 * @param {Object} object The object to invert.
 * @param {boolean} [multiVal] Allow multiple values per key.
 * @param- {Object} [guard] Enables use as an iteratee for functions like `_.map`.
 * @returns {Object} Returns the new inverted object.
 * @example
 *
 * var object = { 'a': 1, 'b': 2, 'c': 1 };
 *
 * _.invert(object);
 * // => { '1': 'c', '2': 'b' }
 *
 * // with `multiVal`
 * _.invert(object, true);
 * // => { '1': ['a', 'c'], '2': ['b'] }
 */
function invert(object, multiVal, guard) {
  return arrayReduce(keys(object), function(result, key) {
    var value = object[key];
    if (multiVal && !guard) {
      if (hasOwnProperty.call(result, value)) {
        result[value].push(key);
      } else {
        result[value] = [key];
      }
    }
    else {
      result[value] = key;
    }
    return result;
  }, {});
}

module.exports = invert;
