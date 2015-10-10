'use strict';

/**
 * ```js
 * var isRelative = require('is-relative');
 * isRelative('README.md');
 * //=> true
 * ```
 *
 * @name isRelative
 * @param {String} `filepath` Path to test.
 * @return {Boolean}
 * @api public
 */

module.exports = function isRelative(filepath) {
  if (typeof filepath !== 'string') {
    throw new Error('isRelative expects a string.');
  }
  return !/^([a-z]+:)?[\\\/]/i.test(filepath);
};