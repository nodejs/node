/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:util:enclose-title
 * @fileoverview Quote a `title` the best way possible.
 */

'use strict';

/* Expose. */
module.exports = enclose;

/**
 * There is currently no way to support nested delimiters
 * across Markdown.pl, CommonMark, and GitHub (RedCarpet).
 * The following code supports Markdown.pl and GitHub.
 * CommonMark is not supported when mixing double- and
 * single quotes inside a title.
 *
 * @see https://github.com/vmg/redcarpet/issues/473
 * @see https://github.com/jgm/CommonMark/issues/308
 *
 * @example
 *   encloseTitle('foo') // '"foo"'
 *   encloseTitle('foo \'bar\' baz') // '"foo \'bar\' baz"'
 *   encloseTitle('foo "bar" baz') // '\'foo "bar" baz\''
 *   encloseTitle('foo "bar" \'baz\'') // '"foo "bar" \'baz\'"'
 *
 * @param {string} title - Content.
 * @return {string} - Properly enclosed title.
 */
function enclose(title) {
  var delimiter = title.indexOf('"') === -1 ? '"' : '\'';
  return delimiter + title + delimiter;
}
