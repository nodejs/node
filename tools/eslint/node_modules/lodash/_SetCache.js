var MapCache = require('./_MapCache'),
    cachePush = require('./_cachePush');

/**
 *
 * Creates a set cache object to store unique values.
 *
 * @private
 * @constructor
 * @param {Array} [values] The values to cache.
 */
function SetCache(values) {
  var index = -1,
      length = values ? values.length : 0;

  this.__data__ = new MapCache;
  while (++index < length) {
    this.push(values[index]);
  }
}

// Add functions to the `SetCache`.
SetCache.prototype.push = cachePush;

module.exports = SetCache;
