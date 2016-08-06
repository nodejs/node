/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module list-item-indent
 * @fileoverview
 *   Warn when the spacing between a list item’s bullet and its content
 *   violates a given style.
 *
 *   Options: `string`, either `'tab-size'`, `'mixed'`, or `'space'`,
 *   default: `'tab-size'`.
 *
 * @example {"name": "valid.md", "setting": "tab-size"}
 *
 *   *   List
 *       item.
 *
 *   Paragraph.
 *
 *   11. List
 *       item.
 *
 *   Paragraph.
 *
 *   *   List
 *       item.
 *
 *   *   List
 *       item.
 *
 * @example {"name": "valid.md", "setting": "mixed"}
 *
 *   * List item.
 *
 *   Paragraph.
 *
 *   11. List item
 *
 *   Paragraph.
 *
 *   *   List
 *       item.
 *
 *   *   List
 *       item.
 *
 * @example {"name": "valid.md", "setting": "space"}
 *
 *   * List item.
 *
 *   Paragraph.
 *
 *   11. List item
 *
 *   Paragraph.
 *
 *   * List
 *     item.
 *
 *   * List
 *     item.
 *
 * @example {"name": "invalid.md", "setting": "invalid", "label": "output", "config": {"positionless": true}}
 *
 *    1:1: Invalid list-item indent style `invalid`: use either `'tab-size'`, `'space'`, or `'mixed'`
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var plural = require('plur');

/* Expose. */
module.exports = listItemIndent;

/* Methods. */
var start = position.start;

/* Styles. */
var STYLES = {
  'tab-size': true,
  'mixed': true,
  'space': true
};

/**
 * Warn when the spacing between a list item’s bullet and
 * its content violates a given style.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string?} [preferred='tab-size'] - Either
 *   `'tab-size'`, `'space'`, or `'mixed'`, defaulting
 *   to the first.
 * @param {Function} done - Callback.
 */
function listItemIndent(ast, file, preferred) {
  var contents = file.toString();

  preferred = typeof preferred === 'string' ? preferred : 'tab-size';

  if (STYLES[preferred] !== true) {
    file.fail('Invalid list-item indent style `' + preferred + '`: use either `\'tab-size\'`, `\'space\'`, or `\'mixed\'`');
  }

  visit(ast, 'list', function (node) {
    var items = node.children;

    if (position.generated(node)) {
      return;
    }

    items.forEach(function (item) {
      var head = item.children[0];
      var initial = start(item).offset;
      var final = start(head).offset;
      var bulletSize;
      var tab;
      var marker;
      var shouldBe;
      var diff;
      var word;

      marker = contents.slice(initial, final);

      /* Support checkboxes. */
      marker = marker.replace(/\[[x ]?\]\s*$/i, '');

      bulletSize = marker.trimRight().length;
      tab = Math.ceil(bulletSize / 4) * 4;

      if (preferred === 'tab-size') {
        shouldBe = tab;
      } else if (preferred === 'space') {
        shouldBe = bulletSize + 1;
      } else {
        shouldBe = node.loose ? tab : bulletSize + 1;
      }

      if (marker.length !== shouldBe) {
        diff = shouldBe - marker.length;
        word = diff > 0 ? 'add' : 'remove';

        diff = Math.abs(diff);

        file.message(
          'Incorrect list-item indent: ' + word +
          ' ' + diff + ' ' + plural('space', diff),
          start(head)
        );
      }
    });
  });
}
