/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module checkbox-character-style
 * @fileoverview
 *   Warn when list item checkboxes violate a given style.
 *
 *   The default value, `consistent`, detects the first used checked
 *   and unchecked checkbox styles, and will warn when a subsequent
 *   checkboxes uses a different style.
 *
 *   These values can also be passed in as an object, such as:
 *
 *   ```js
 *   {checked: 'x', unchecked: ' '}
 *   ```
 *
 * @example {"name": "valid.md", "setting": {"checked": "x"}}
 *
 *   <!--This file is also valid by default-->
 *
 *   - [x] List item
 *   - [x] List item
 *
 * @example {"name": "valid.md", "setting": {"checked": "X"}}
 *
 *   <!--This file is also valid by default-->
 *
 *   - [X] List item
 *   - [X] List item
 *
 * @example {"name": "valid.md", "setting": {"unchecked": " "}}
 *
 *   <!--This file is also valid by default-->
 *
 *   - [ ] List item
 *   - [ ] List item
 *   - [ ]··
 *   - [ ]
 *
 * @example {"name": "valid.md", "setting": {"unchecked": "\t"}}
 *
 *   <!--Also valid by default (note: `»` represents `\t`)-->
 *
 *   - [»] List item
 *   - [»] List item
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   <!--Note: `»` represents `\t`-->
 *
 *   - [x] List item
 *   - [X] List item
 *   - [ ] List item
 *   - [»] List item
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   4:4-4:5: Checked checkboxes should use `x` as a marker
 *   6:4-6:5: Unchecked checkboxes should use ` ` as a marker
 *
 * @example {"setting": {"unchecked": "!"}, "name": "invalid.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Invalid unchecked checkbox marker `!`: use either `'\t'`, or `' '`
 *
 * @example {"setting": {"checked": "!"}, "name": "invalid.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Invalid checked checkbox marker `!`: use either `'x'`, or `'X'`
 */

'use strict';

/* Dependencies. */
var vfileLocation = require('vfile-location');
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = checkboxCharacterStyle;

/* Methods. */
var start = position.start;
var end = position.end;

/* Constants. */
var CHECKED = {x: true, X: true};
var UNCHECKED = {' ': true, '\t': true};

/**
 * Warn when list item checkboxes violate a given style.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {Object?} preferred - An object with `checked`
 *   and `unchecked` properties, each set to null to default to
 *   the first found style, or set to `'x'` or `'X'` for checked,
 *   or `' '` (space) or `'\t'` (tab) for unchecked.
 */
function checkboxCharacterStyle(ast, file, preferred) {
  var contents = file.toString();
  var location = vfileLocation(file);

  if (preferred === 'consistent' || typeof preferred !== 'object') {
    preferred = {};
  }

  if (!preferred.unchecked) {
    preferred.unchecked = null;
  }

  if (!preferred.checked) {
    preferred.checked = null;
  }

  if (
    preferred.unchecked !== null &&
    UNCHECKED[preferred.unchecked] !== true
  ) {
    file.fail(
      'Invalid unchecked checkbox marker `' +
      preferred.unchecked +
      '`: use either `\'\\t\'`, or `\' \'`'
    );
  }

  if (
    preferred.checked !== null &&
    CHECKED[preferred.checked] !== true
  ) {
    file.fail(
      'Invalid checked checkbox marker `' +
      preferred.checked +
      '`: use either `\'x\'`, or `\'X\'`'
    );
  }

  visit(ast, 'listItem', function (node) {
    var type;
    var initial;
    var final;
    var stop;
    var value;
    var style;
    var character;

    /* Exit early for items without checkbox. */
    if (
        node.checked !== Boolean(node.checked) ||
        position.generated(node)
    ) {
      return;
    }

    type = node.checked ? 'checked' : 'unchecked';

    initial = start(node).offset;
    final = (node.children.length ? start(node.children[0]) : end(node)).offset;

    /* For a checkbox to be parsed, it must be followed
     * by a white space. */
    value = contents.slice(initial, final).trimRight().slice(0, -1);

    /* The checkbox character is behind a square
     * bracket. */
    character = value.charAt(value.length - 1);
    style = preferred[type];

    if (style === null) {
      preferred[type] = character;
    } else if (character !== style) {
      stop = initial + value.length;

      file.message(
        type.charAt(0).toUpperCase() + type.slice(1) +
        ' checkboxes should use `' + style + '` as a marker',
        {
          start: location.toPosition(stop - 1),
          end: location.toPosition(stop)
        }
      );
    }
  });
}
