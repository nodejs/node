var LazyWrapper = require('../internal/LazyWrapper'),
    LodashWrapper = require('../internal/LodashWrapper'),
    thru = require('./thru');

/**
 * Reverses the wrapped array so the first element becomes the last, the
 * second element becomes the second to last, and so on.
 *
 * **Note:** This method mutates the wrapped array.
 *
 * @name reverse
 * @memberOf _
 * @category Chain
 * @returns {Object} Returns the new reversed `lodash` wrapper instance.
 * @example
 *
 * var array = [1, 2, 3];
 *
 * _(array).reverse().value()
 * // => [3, 2, 1]
 *
 * console.log(array);
 * // => [3, 2, 1]
 */
function wrapperReverse() {
  var value = this.__wrapped__;

  var interceptor = function(value) {
    return value.reverse();
  };
  if (value instanceof LazyWrapper) {
    var wrapped = value;
    if (this.__actions__.length) {
      wrapped = new LazyWrapper(this);
    }
    wrapped = wrapped.reverse();
    wrapped.__actions__.push({ 'func': thru, 'args': [interceptor], 'thisArg': undefined });
    return new LodashWrapper(wrapped, this.__chain__);
  }
  return this.thru(interceptor);
}

module.exports = wrapperReverse;
