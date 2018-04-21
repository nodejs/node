var LazyWrapper = require('./LazyWrapper'),
    arrayCopy = require('./arrayCopy');

/**
 * Creates a clone of the lazy wrapper object.
 *
 * @private
 * @name clone
 * @memberOf LazyWrapper
 * @returns {Object} Returns the cloned `LazyWrapper` object.
 */
function lazyClone() {
  var result = new LazyWrapper(this.__wrapped__);
  result.__actions__ = arrayCopy(this.__actions__);
  result.__dir__ = this.__dir__;
  result.__filtered__ = this.__filtered__;
  result.__iteratees__ = arrayCopy(this.__iteratees__);
  result.__takeCount__ = this.__takeCount__;
  result.__views__ = arrayCopy(this.__views__);
  return result;
}

module.exports = lazyClone;
