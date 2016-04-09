var isSymbol = require('./isSymbol');

/**
 * Casts `value` to a string if it's not a string or symbol.
 *
 * @private
 * @param {*} value The value to inspect.
 * @returns {string|symbol} Returns the cast key.
 */
function baseCastKey(key) {
  return (typeof key == 'string' || isSymbol(key)) ? key : (key + '');
}

module.exports = baseCastKey;
