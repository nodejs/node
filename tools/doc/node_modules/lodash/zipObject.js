var baseSet = require('./internal/baseSet');

/**
 * This method is like `_.fromPairs` except that it accepts two arrays,
 * one of property names and one of corresponding values.
 *
 * @static
 * @memberOf _
 * @category Array
 * @param {Array} [props=[]] The property names.
 * @param {Array} [values=[]] The property values.
 * @returns {Object} Returns the new object.
 * @example
 *
 * _.zipObject(['fred', 'barney'], [30, 40]);
 * // => { 'fred': 30, 'barney': 40 }
 */
function zipObject(props, values) {
  var index = -1,
      length = props ? props.length : 0,
      valsLength = values ? values.length : 0,
      result = {};

  while (++index < length) {
    baseSet(result, props[index], index < valsLength ? values[index] : undefined);
  }
  return result;
}

module.exports = zipObject;
