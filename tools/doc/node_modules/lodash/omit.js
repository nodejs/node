var arrayMap = require('./internal/arrayMap'),
    baseDifference = require('./internal/baseDifference'),
    baseFlatten = require('./internal/baseFlatten'),
    basePick = require('./internal/basePick'),
    keysIn = require('./keysIn'),
    rest = require('./rest');

/**
 * The opposite of `_.pick`; this method creates an object composed of the
 * own and inherited enumerable properties of `object` that are not omitted.
 *
 * @static
 * @memberOf _
 * @category Object
 * @param {Object} object The source object.
 * @param {...(string|string[])} [props] The property names to omit, specified
 *  individually or in arrays..
 * @returns {Object} Returns the new object.
 * @example
 *
 * var object = { 'a': 1, 'b': '2', 'c': 3 };
 *
 * _.omit(object, ['a', 'c']);
 * // => { 'b': '2' }
 */
var omit = rest(function(object, props) {
  if (object == null) {
    return {};
  }
  props = arrayMap(baseFlatten(props), String);
  return basePick(object, baseDifference(keysIn(object), props));
});

module.exports = omit;
