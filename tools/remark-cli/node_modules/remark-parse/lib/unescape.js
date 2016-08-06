/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:unescape
 * @fileoverview Unescape escapes.
 */

'use strict';

/* Expose. */
module.exports = factory;

/**
 * Factory to de-escape a value, based on a list at `key`
 * in `ctx`.
 *
 * @example
 *   var ctx = {escape: ['a']}
 *   var unescape = unescapeFactory(ctx, 'escape');
 *
 * @param {Object} ctx - List of escapable characters.
 * @param {string} key - Key in `map` at which the list
 *   exists.
 * @return {function(string): string} - Function which
 *   takes a value and returns its unescaped version.
 */
function factory(ctx, key) {
  return unescape;

  /**
   * De-escape a string using the expression at `key`
   * in `ctx`.
   *
   * @example
   *   var ctx = {escape: ['a']}
   *   var unescape = unescapeFactory(ctx, 'escape');
   *   unescape('\a \b'); // 'a \b'
   *
   * @param {string} value - Escaped string.
   * @return {string} - Unescaped string.
   */
  function unescape(value) {
    var prev = 0;
    var index = value.indexOf('\\');
    var escape = ctx[key];
    var queue = [];
    var character;

    while (index !== -1) {
      queue.push(value.slice(prev, index));
      prev = index + 1;
      character = value.charAt(prev);

      /* If the following character is not a valid escape,
       * add the slash. */
      if (!character || escape.indexOf(character) === -1) {
        queue.push('\\');
      }

      index = value.indexOf('\\', prev);
    }

    queue.push(value.slice(prev));

    return queue.join('');
  }
}
