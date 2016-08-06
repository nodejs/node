/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-consecutive-blank-lines
 * @fileoverview
 *   Warn for too many consecutive blank lines.  Knows about the extra line
 *   needed between a list and indented code, and two lists.
 *
 * @example {"name": "valid.md"}
 *
 *   Foo...
 *
 *   ...Bar.
 *
 * @example {"name": "valid-for-code.md"}
 *
 *   Paragraph.
 *
 *   *   List
 *
 *
 *       bravo();
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   Foo...
 *
 *
 *   ...Bar.
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   4:1: Remove 1 line before node
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var plural = require('plur');

/* Expose. */
module.exports = noConsecutiveBlankLines;

/* Constants. */
var MAX = 2;

/**
 * Warn for too many consecutive blank lines.  Knows
 * about the extra line needed between a list and
 * indented code, and two lists.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noConsecutiveBlankLines(ast, file) {
  visit(ast, function (node) {
    var children = node.children;
    var head = children && children[0];
    var tail = children && children[children.length - 1];

    if (position.generated(node)) {
      return;
    }

    if (head && !position.generated(head)) {
      /* Compare parent and first child. */
      compare(position.start(node), position.start(head), 0);

      /* Compare between each child. */
      children.forEach(function (child, index) {
        var prev = children[index - 1];
        var max = MAX;

        if (
          !prev ||
          position.generated(prev) ||
          position.generated(child)
        ) {
          return;
        }

        if (
          (prev.type === 'list' && child.type === 'list') ||
          (child.type === 'code' && prev.type === 'list' && !child.lang)
        ) {
          max++;
        }

        compare(position.end(prev), position.start(child), max);
      });

      /* Compare parent and last child. */
      if (tail !== head && !position.generated(tail)) {
        compare(position.end(node), position.end(tail), 1);
      }
    }
  });

  return;

  /**
   * Compare the difference between `start` and `end`,
   * and warn when that difference exceeds `max`.
   *
   * @param {Position} start - Initial.
   * @param {Position} end - Final.
   * @param {number} max - Threshold.
   */
  function compare(start, end, max) {
    var diff = end.line - start.line;
    var word = diff > 0 ? 'before' : 'after';

    diff = Math.abs(diff) - max;

    if (diff > 0) {
      file.message('Remove ' + diff + ' ' + plural('line', diff) + ' ' + word + ' node', end);
    }
  }
}
