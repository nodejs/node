/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:code
 * @fileoverview Stringify code.
 */

'use strict';

/* Dependencies. */
var streak = require('longest-streak');
var repeat = require('repeat-string');
var pad = require('../util/pad');

/* Expose. */
module.exports = code;

/* Constants. */
var FENCE = /([`~])\1{2}/;

/**
 * Stringify code.
 *
 * Creates indented code when:
 *
 * - No language tag exists;
 * - Not in `fences: true` mode;
 * - A non-empty value exists.
 *
 * Otherwise, GFM fenced code is created:
 *
 *     ```js
 *     foo();
 *     ```
 *
 * When in ``fence: `~` `` mode, uses tildes as fences:
 *
 *     ~~~js
 *     foo();
 *     ~~~
 *
 * Knows about internal fences (Note: GitHub/Kramdown does
 * not support this):
 *
 *     ````javascript
 *     ```markdown
 *     foo
 *     ```
 *     ````
 *
 * Supports named entities in the language flag with
 * `settings.encode` mode.
 *
 * @param {Object} node - `code` node.
 * @param {Object} parent - Parent of `node`.
 * @return {string} - Markdown code.
 */
function code(node, parent) {
  var self = this;
  var value = node.value;
  var options = self.options;
  var marker = options.fence;
  var language = self.encode(node.lang || '', node);
  var fence;

  /* Without (needed) fences. */
  if (!language && !options.fences && value) {
    /* Throw when pedantic, in a list item which
     * isn’t compiled using a tab. */
    if (
      parent &&
      parent.type === 'listItem' &&
      options.listItemIndent !== 'tab' &&
      options.pedantic
    ) {
      self.file.fail(
        'Cannot indent code properly. See http://git.io/vgFvT',
        node.position
      );
    }

    return pad(value, 1);
  }

  fence = streak(value, marker) + 1;

  /* Fix GFM / RedCarpet bug, where fence-like characters
   * inside fenced code can exit a code-block.
   * Yes, even when the outer fence uses different
   * characters, or is longer.
   * Thus, we can only pad the code to make it work. */
  if (FENCE.test(value)) {
    value = pad(value, 1);
  }

  fence = repeat(marker, Math.max(fence, 3));

  return fence + language + '\n' + value + '\n' + fence;
}
