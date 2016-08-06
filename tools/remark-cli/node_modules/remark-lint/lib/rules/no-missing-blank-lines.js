/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-missing-blank-lines
 * @fileoverview
 *   Warn when missing blank lines before a block node.
 *
 *   This rule can be configured to allow tight list items
 *   without blank lines between their contents through
 *   `exceptTightLists: true` (default: false).
 *
 * @example {"name": "valid.md"}
 *
 *   # Foo
 *
 *   ## Bar
 *
 *   - Paragraph
 *
 *     + List.
 *
 *   Paragraph.
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   # Foo
 *   ## Bar
 *
 *   - Paragraph
 *     + List.
 *
 *   Paragraph.
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   2:1-2:7: Missing blank line before block node
 *   5:3-5:10: Missing blank line before block node
 *
 * @example {"name": "tight.md", "setting": {"exceptTightLists": true}, "label": "input"}
 *
 *   # Foo
 *   ## Bar
 *
 *   - Paragraph
 *     + List.
 *
 *   Paragraph.
 *
 * @example {"name": "tight.md", "setting": {"exceptTightLists": true}, "label": "output"}
 *
 *   2:1-2:7: Missing blank line before block node
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = noMissingBlankLines;

/**
 * Warn when there is no empty line between two block
 * nodes.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noMissingBlankLines(ast, file, options) {
  var allow = (options || {}).exceptTightLists;

  visit(ast, function (node, index, parent) {
    var next = parent && parent.children[index + 1];

    if (position.generated(node)) {
      return;
    }

    if (allow && parent && parent.type === 'listItem') {
      return;
    }

    if (
      next &&
      applicable(node) &&
      applicable(next) &&
      position.start(next).line === position.end(node).line + 1
    ) {
      file.message('Missing blank line before block node', next);
    }
  });
}

/**
 * Check if `node` is an applicable block-level node.
 *
 * @param {Node} node - Node to test.
 * @return {boolean} - Whether or not `node` is applicable.
 */
function applicable(node) {
  return [
    'paragraph',
    'blockquote',
    'heading',
    'code',
    'yaml',
    'html',
    'list',
    'table',
    'thematicBreak'
  ].indexOf(node.type) !== -1;
}
