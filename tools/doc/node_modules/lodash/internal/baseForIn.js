var baseFor = require('./baseFor'),
    keysIn = require('../keysIn');

/**
 * The base implementation of `_.forIn` without support for iteratee shorthands.
 *
 * @private
 * @param {Object} object The object to iterate over.
 * @param {Function} iteratee The function invoked per iteration.
 * @returns {Object} Returns `object`.
 */
function baseForIn(object, iteratee) {
  return object == null ? object : baseFor(object, iteratee, keysIn);
}

module.exports = baseForIn;
