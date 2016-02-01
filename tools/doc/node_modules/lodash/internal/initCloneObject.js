var baseCreate = require('./baseCreate'),
    isFunction = require('../isFunction');

/**
 * Initializes an object clone.
 *
 * @private
 * @param {Object} object The object to clone.
 * @returns {Object} Returns the initialized clone.
 */
function initCloneObject(object) {
  var Ctor = object.constructor;
  return baseCreate(isFunction(Ctor) ? Ctor.prototype : undefined);
}

module.exports = initCloneObject;
