/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module code-block-style
 * @fileoverview
 *   Warn when code-blocks do not adhere to a given style.
 *
 *   Options: `string`, either `'consistent'`, `'fenced'`, or `'indented'`,
 *   default: `'consistent'`.
 *
 *   The default value, `consistent`, detects the first used code-block
 *   style, and will warn when a subsequent code-block uses a different
 *   style.
 *
 * @example {"setting": "indented", "name": "valid.md"}
 *
 *   <!-- This is also valid when `'consistent'` -->
 *
 *       alpha();
 *
 *   Paragraph.
 *
 *       bravo();
 *
 * @example {"setting": "indented", "name": "invalid.md", "label": "input"}
 *
 *   ```
 *   alpha();
 *   ```
 *
 *   Paragraph.
 *
 *   ```
 *   bravo();
 *   ```
 *
 * @example {"setting": "indented", "name": "invalid.md", "label": "output"}
 *
 *   1:1-3:4: Code blocks should be indented
 *   7:1-9:4: Code blocks should be indented
 *
 * @example {"setting": "fenced", "name": "valid.md"}
 *
 *   <!-- This is also valid when `'consistent'` -->
 *
 *   ```
 *   alpha();
 *   ```
 *
 *   Paragraph.
 *
 *   ```
 *   bravo();
 *   ```
 *
 * @example {"setting": "fenced", "name": "invalid.md", "label": "input"}
 *
 *       alpha();
 *
 *   Paragraph.
 *
 *       bravo();
 *
 * @example {"setting": "fenced", "name": "invalid.md", "label": "output"}
 *
 *   1:1-1:13: Code blocks should be fenced
 *   5:1-5:13: Code blocks should be fenced
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   <!-- This is always invalid -->
 *
 *       alpha();
 *
 *   Paragraph.
 *
 *   ```
 *   bravo();
 *   ```
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   7:1-9:4: Code blocks should be indented
 *
 * @example {"setting": "invalid", "name": "invalid.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Invalid code block style `invalid`: use either `'consistent'`, `'fenced'`, or `'indented'`
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = codeBlockStyle;

/* Methods. */
var start = position.start;
var end = position.end;

/* Valid styles. */
var STYLES = {
  null: true,
  fenced: true,
  indented: true
};

/**
 * Warn for violating code-block style.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string?} [preferred='consistent'] - Preferred
 *   code block style.  Defaults to `'consistent'` when
 *   not a a string.  Otherwise, should be one of
 *   `'fenced'` or `'indented'`.
 */
function codeBlockStyle(ast, file, preferred) {
  var contents = file.toString();

  preferred = typeof preferred !== 'string' || preferred === 'consistent' ? null : preferred;

  if (STYLES[preferred] !== true) {
    file.fail('Invalid code block style `' + preferred + '`: use either `\'consistent\'`, `\'fenced\'`, or `\'indented\'`');
  }

  visit(ast, 'code', function (node) {
    var current = check(node);

    if (!current) {
      return;
    }

    if (!preferred) {
      preferred = current;
    } else if (preferred !== current) {
      file.message('Code blocks should be ' + preferred, node);
    }
  });

  return;

  /**
   * Get the style of `node`.
   *
   * @param {Node} node - Node.
   * @return {string?} - `'fenced'`, `'indented'`, or
   *   `null`.
   */
  function check(node) {
    var initial = start(node).offset;
    var final = end(node).offset;

    if (position.generated(node)) {
      return null;
    }

    if (
      node.lang ||
      /^\s*([~`])\1{2,}/.test(contents.slice(initial, final))
    ) {
      return 'fenced';
    }

    return 'indented';
  }
}
