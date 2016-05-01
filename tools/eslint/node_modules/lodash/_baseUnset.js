var castPath = require('./_castPath'),
    has = require('./has'),
    isKey = require('./_isKey'),
    last = require('./last'),
    parent = require('./_parent');

/**
 * The base implementation of `_.unset`.
 *
 * @private
 * @param {Object} object The object to modify.
 * @param {Array|string} path The path of the property to unset.
 * @returns {boolean} Returns `true` if the property is deleted, else `false`.
 */
function baseUnset(object, path) {
  path = isKey(path, object) ? [path] : castPath(path);
  object = parent(object, path);
  var key = last(path);
  return (object != null && has(object, key)) ? delete object[key] : true;
}

module.exports = baseUnset;
