var arrayMap = require('./_arrayMap'),
    baseCastKey = require('./_baseCastKey'),
    copyArray = require('./_copyArray'),
    isArray = require('./isArray'),
    isSymbol = require('./isSymbol'),
    stringToPath = require('./_stringToPath');

/**
 * Converts `value` to a property path array.
 *
 * @static
 * @memberOf _
 * @since 4.0.0
 * @category Util
 * @param {*} value The value to convert.
 * @returns {Array} Returns the new property path array.
 * @example
 *
 * _.toPath('a.b.c');
 * // => ['a', 'b', 'c']
 *
 * _.toPath('a[0].b.c');
 * // => ['a', '0', 'b', 'c']
 *
 * var path = ['a', 'b', 'c'],
 *     newPath = _.toPath(path);
 *
 * console.log(newPath);
 * // => ['a', 'b', 'c']
 *
 * console.log(path === newPath);
 * // => false
 */
function toPath(value) {
  if (isArray(value)) {
    return arrayMap(value, baseCastKey);
  }
  return isSymbol(value) ? [value] : copyArray(stringToPath(value));
}

module.exports = toPath;
