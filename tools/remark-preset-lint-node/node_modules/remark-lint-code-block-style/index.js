/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module code-block-style
 * @fileoverview
 *   Warn when code-blocks do not adhere to a given style.
 *
 *   Options: `'consistent'`, `'fenced'`, or `'indented'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used code-block style and warns when
 *   subsequent code-blocks uses different styles.
 *
 * @example {"setting": "indented", "name": "valid.md"}
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
 *   5:1-7:4: Code blocks should be indented
 *
 * @example {"setting": "invalid", "name": "invalid.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Invalid code block style `invalid`: use either `'consistent'`, `'fenced'`, or `'indented'`
 */

'use strict';

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:code-block-style', codeBlockStyle);

var start = position.start;
var end = position.end;

var STYLES = {
  null: true,
  fenced: true,
  indented: true
};

function codeBlockStyle(tree, file, preferred) {
  var contents = file.toString();

  preferred = typeof preferred !== 'string' || preferred === 'consistent' ? null : preferred;

  if (STYLES[preferred] !== true) {
    file.fail('Invalid code block style `' + preferred + '`: use either `\'consistent\'`, `\'fenced\'`, or `\'indented\'`');
  }

  visit(tree, 'code', visitor);

  function visitor(node) {
    var current = check(node);

    if (!current) {
      return;
    }

    if (!preferred) {
      preferred = current;
    } else if (preferred !== current) {
      file.message('Code blocks should be ' + preferred, node);
    }
  }

  /* Get the style of `node`. */
  function check(node) {
    var initial = start(node).offset;
    var final = end(node).offset;

    if (generated(node)) {
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
