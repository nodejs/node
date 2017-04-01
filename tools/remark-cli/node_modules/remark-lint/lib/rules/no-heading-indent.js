/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-heading-indent
 * @fileoverview
 *   Warn when a heading is indented.
 *
 * @example {"name": "valid.md"}
 *
 *   <!-- Note: the middle-dots represent spaces -->
 *
 *   #·Hello world
 *
 *   Foo
 *   -----
 *
 *   #·Hello world·#
 *
 *   Bar
 *   =====
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   <!-- Note: the middle-dots represent spaces -->
 *
 *   ···# Hello world
 *
 *   ·Foo
 *   -----
 *
 *   ·# Hello world #
 *
 *   ···Bar
 *   =====
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:4: Remove 3 spaces before this heading
 *   5:2: Remove 1 space before this heading
 *   8:2: Remove 1 space before this heading
 *   10:4: Remove 3 spaces before this heading
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var plural = require('plur');
var position = require('unist-util-position');

/* Expose. */
module.exports = noHeadingIndent;

/* Methods. */
var start = position.start;

/**
 * Warn when a heading has too much space before the
 * initial hashes.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noHeadingIndent(ast, file) {
  var contents = file.toString();
  var length = contents.length;

  visit(ast, 'heading', function (node) {
    var initial = start(node);
    var begin = initial.offset;
    var index = begin - 1;
    var character;
    var diff;

    if (position.generated(node)) {
      return;
    }

    while (++index < length) {
      character = contents.charAt(index);

      if (character !== ' ' && character !== '\t') {
        break;
      }
    }

    diff = index - begin;

    if (diff) {
      file.message(
        'Remove ' + diff + ' ' + plural('space', diff) +
        ' before this heading',
        {
          line: initial.line,
          column: initial.column + diff
        }
      );
    }
  });
}
