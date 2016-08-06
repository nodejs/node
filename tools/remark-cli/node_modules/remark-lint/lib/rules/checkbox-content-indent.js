/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module checkbox-content-indent
 * @fileoverview
 *   Warn when list item checkboxes are followed by too much white-space.
 *
 * @example {"name": "valid.md"}
 *
 *   - [ ] List item
 *   +  [x] List item
 *   *   [X] List item
 *   -    [ ] List item
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   - [ ] List item
 *   + [x]  List item
 *   * [X]   List item
 *   - [ ]    List item
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   2:7-2:8: Checkboxes should be followed by a single character
 *   3:7-3:9: Checkboxes should be followed by a single character
 *   4:7-4:10: Checkboxes should be followed by a single character
 */

'use strict';

/* Dependencies. */
var vfileLocation = require('vfile-location');
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = checkboxContentIndent;

/* Methods. */
var start = position.start;
var end = position.end;

/**
 * Warn when list item checkboxes are followed by too much white-space.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {*} preferred - Ignored.
 */
function checkboxContentIndent(ast, file) {
  var contents = file.toString();
  var location = vfileLocation(file);

  visit(ast, 'listItem', function (node) {
    var initial;
    var final;
    var value;

    /* Exit early for items without checkbox. */
    if (
      node.checked !== Boolean(node.checked) ||
      position.generated(node)
    ) {
      return;
    }

    initial = start(node).offset;
    final = (node.children.length ? start(node.children[0]) : end(node)).offset;

    while (/[^\S\n]/.test(contents.charAt(final))) {
      final++;
    }

    /* For a checkbox to be parsed, it must be followed
     * by a white space. */
    value = contents.slice(initial, final);

    value = value.slice(value.indexOf(']') + 1);

    if (value.length === 1) {
      return;
    }

    file.message('Checkboxes should be followed by a single character', {
      start: location.toPosition(final - value.length + 1),
      end: location.toPosition(final)
    });
  });
}
