/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module fenced-code-flag
 * @fileoverview
 *   Warn when fenced code blocks occur without language flag.
 *
 *   Options: `Array.<string>` or `Object`.
 *
 *   Providing an array, is a shortcut for just providing the `flags`
 *   property on the object.
 *
 *   The object can have an array of flags which are deemed valid.
 *   In addition it can have the property `allowEmpty` (`boolean`)
 *   which signifies whether or not to warn for fenced code-blocks without
 *   languge flags.
 *
 * @example {"name": "valid.md"}
 *
 *   ```alpha
 *   bravo();
 *   ```
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   ```
 *   alpha();
 *   ```
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-3:4: Missing code-language flag
 *
 * @example {"name": "valid.md", "setting": {"allowEmpty": true}}
 *
 *   ```
 *   alpha();
 *   ```
 *
 * @example {"name": "invalid.md", "setting": {"allowEmpty": false}, "label": "input"}
 *
 *   ```
 *   alpha();
 *   ```
 *
 * @example {"name": "invalid.md", "setting": {"allowEmpty": false}, "label": "output"}
 *
 *   1:1-3:4: Missing code-language flag
 *
 * @example {"name": "valid.md", "setting": ["alpha"]}
 *
 *   ```alpha
 *   bravo();
 *   ```
 *
 * @example {"name": "invalid.md", "setting": ["charlie"], "label": "input"}
 *
 *   ```alpha
 *   bravo();
 *   ```
 *
 * @example {"name": "invalid.md", "setting": ["charlie"], "label": "output"}
 *
 *   1:1-3:4: Invalid code-language flag
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = fencedCodeFlag;

/* Methods. */
var start = position.start;
var end = position.end;

/**
 * Warn for fenced code blocks without language flag.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {Object|Array.<string>} [preferred] - List
 *   of flags deemed valid.
 */
function fencedCodeFlag(ast, file, preferred) {
  var contents = file.toString();
  var allowEmpty = false;
  var flags = [];

  if (typeof preferred === 'object' && !('length' in preferred)) {
    allowEmpty = Boolean(preferred.allowEmpty);

    preferred = preferred.flags;
  }

  if (typeof preferred === 'object' && 'length' in preferred) {
    flags = String(preferred).split(',');
  }

  visit(ast, 'code', function (node) {
    var value = contents.slice(start(node).offset, end(node).offset);

    if (position.generated(node)) {
      return;
    }

    if (node.lang) {
      if (flags.length && flags.indexOf(node.lang) === -1) {
        file.message('Invalid code-language flag', node);
      }
    } else if (/^ {0,3}([~`])\1{2,}/.test(value) && !allowEmpty) {
      file.message('Missing code-language flag', node);
    }
  });
}
