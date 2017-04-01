/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:util:enclose-uri
 * @fileoverview Wrap `url` in angle brackets when needed.
 */

'use strict';

/* Dependencies. */
var count = require('ccount');

/* Expose. */
module.exports = enclose;

/* Constants. */
var re = /\s/;

/**
 * Wrap `url` in angle brackets when needed, or when
 * forced.
 *
 * In links, images, and definitions, the URL part needs
 * to be enclosed when it:
 *
 * - has a length of `0`;
 * - contains white-space;
 * - has more or less opening than closing parentheses.
 *
 * @example
 *   encloseURI('foo bar') // '<foo bar>'
 *   encloseURI('foo(bar(baz)') // '<foo(bar(baz)>'
 *   encloseURI('') // '<>'
 *   encloseURI('example.com') // 'example.com'
 *   encloseURI('example.com', true) // '<example.com>'
 *
 * @param {string} uri - URI to enclose.
 * @param {boolean?} [always] - Force enclosing.
 * @return {boolean} - Properly enclosed `uri`.
 */
function enclose(uri, always) {
  if (always || !uri.length || re.test(uri) || count(uri, '(') !== count(uri, ')')) {
    return '<' + uri + '>';
  }

  return uri;
}
