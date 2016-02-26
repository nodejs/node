var baseCreate = require('./_baseCreate'),
    isFunction = require('./isFunction'),
    isPrototype = require('./_isPrototype');

/** Built-in value references. */
var getPrototypeOf = Object.getPrototypeOf;

/**
 * Initializes an object clone.
 *
 * @private
 * @param {Object} object The object to clone.
 * @returns {Object} Returns the initialized clone.
 */
function initCloneObject(object) {
  return (isFunction(object.constructor) && !isPrototype(object))
    ? baseCreate(getPrototypeOf(object))
    : {};
}

module.exports = initCloneObject;
