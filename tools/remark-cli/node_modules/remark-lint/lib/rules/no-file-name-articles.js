/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-articles
 * @fileoverview
 *   Warn when file name start with an article.
 *
 * @example {"name": "title.md"}
 *
 * @example {"name": "a-title.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not start file names with `a`
 *
 * @example {"name": "the-title.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not start file names with `the`
 *
 * @example {"name": "an-article.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not start file names with `an`
 */

'use strict';

/* Expose. */
module.exports = noFileNameArticles;

/**
 * Warn when file name start with an article.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noFileNameArticles(ast, file) {
  var match = file.stem && file.stem.match(/^(the|an?)\b/i);

  if (match) {
    file.message('Do not start file names with `' + match[0] + '`');
  }
}
