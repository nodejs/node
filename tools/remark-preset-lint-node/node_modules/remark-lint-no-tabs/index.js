/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-tabs
 * @fileoverview
 *   Warn when hard tabs are used instead of spaces.
 *
 * @example {"name": "valid.md"}
 *
 *   Foo Bar
 *
 *   ····Foo
 *
 * @example {"name": "invalid.md", "label": "input", "config": {"positionless": true}}
 *
 *   »Here's one before a code block.
 *
 *   Here's a tab:», and here is another:».
 *
 *   And this is in `inline»code`.
 *
 *   >»This is in a block quote.
 *
 *   *»And...
 *
 *   »1.»in a list.
 *
 *   And this is a tab as the last character.»
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1: Use spaces instead of hard-tabs
 *   3:14: Use spaces instead of hard-tabs
 *   3:37: Use spaces instead of hard-tabs
 *   5:23: Use spaces instead of hard-tabs
 *   7:2: Use spaces instead of hard-tabs
 *   9:2: Use spaces instead of hard-tabs
 *   11:1: Use spaces instead of hard-tabs
 *   11:4: Use spaces instead of hard-tabs
 *   13:41: Use spaces instead of hard-tabs
 */

'use strict';

var rule = require('unified-lint-rule');
var location = require('vfile-location');

module.exports = rule('remark-lint:no-tabs', noTabs);

function noTabs(ast, file) {
  var content = file.toString();
  var position = location(file).toPosition;
  var index = -1;
  var length = content.length;

  while (++index < length) {
    if (content.charAt(index) === '\t') {
      file.message('Use spaces instead of hard-tabs', position(index));
    }
  }
}
