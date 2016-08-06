/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-tabs
 * @fileoverview
 *   Warn when hard-tabs instead of spaces
 *
 * @example {"name": "valid.md"}
 *
 *   Foo Bar
 *
 *       Foo
 *
 * @example {"name": "invalid.md", "label": "input", "config": {"positionless": true}}
 *
 *   <!-- Note: the guillemets represent tabs -->
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
 *   3:1: Use spaces instead of hard-tabs
 *   5:14: Use spaces instead of hard-tabs
 *   5:37: Use spaces instead of hard-tabs
 *   7:23: Use spaces instead of hard-tabs
 *   9:2: Use spaces instead of hard-tabs
 *   11:2: Use spaces instead of hard-tabs
 *   13:1: Use spaces instead of hard-tabs
 *   13:4: Use spaces instead of hard-tabs
 *   15:41: Use spaces instead of hard-tabs
 */

'use strict';

/* Dependencies. */
var vfileLocation = require('vfile-location');

/* Expose. */
module.exports = noTabs;

/**
 * Warn when hard-tabs instead of spaces are used.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {*} preferred - Ignored.
 * @param {Function} done - Callback.
 */
function noTabs(ast, file, preferred, done) {
  var content = file.toString();
  var location = vfileLocation(file);
  var index = -1;
  var length = content.length;

  while (++index < length) {
    if (content.charAt(index) === '\t') {
      file.message('Use spaces instead of hard-tabs', location.toPosition(index));
    }
  }

  done();
}
