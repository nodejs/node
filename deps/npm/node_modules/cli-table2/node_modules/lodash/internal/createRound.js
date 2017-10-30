/** Native method references. */
var pow = Math.pow;

/**
 * Creates a `_.ceil`, `_.floor`, or `_.round` function.
 *
 * @private
 * @param {string} methodName The name of the `Math` method to use when rounding.
 * @returns {Function} Returns the new round function.
 */
function createRound(methodName) {
  var func = Math[methodName];
  return function(number, precision) {
    precision = precision === undefined ? 0 : (+precision || 0);
    if (precision) {
      precision = pow(10, precision);
      return func(number * precision) / precision;
    }
    return func(number);
  };
}

module.exports = createRound;
