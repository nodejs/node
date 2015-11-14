var baseConvert = require('./_baseConvert');

/**
 * Converts `lodash` to an immutable auto-curried iteratee-first data-last version.
 *
 * @param {Function} lodash The lodash function.
 * @returns {Function} Returns the converted `lodash`.
 */
function browserConvert(lodash) {
  return baseConvert(lodash, lodash);
}

module.exports = browserConvert;
