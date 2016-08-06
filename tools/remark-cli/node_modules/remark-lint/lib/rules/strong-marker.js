/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module strong-marker
 * @fileoverview
 *   Warn for violating strong markers.
 *
 *   Options: `string`, either `'consistent'`, `'*'`, or `'_'`,
 *   default: `'consistent'`.
 *
 *   The default value, `consistent`, detects the first used strong
 *   style, and will warn when a subsequent strong uses a different
 *   style.
 *
 * @example {"name": "valid.md"}
 *
 *   **foo** and **bar**.
 *
 * @example {"name": "also-valid.md"}
 *
 *   __foo__ and __bar__.
 *
 * @example {"name": "valid.md", "setting": "*"}
 *
 *   **foo**.
 *
 * @example {"name": "valid.md", "setting": "_"}
 *
 *   __foo__.
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   **foo** and __bar__.
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:13-1:20: Strong should use `*` as a marker
 *
 * @example {"name": "invalid.md", "label": "output", "setting": "!", "config": {"positionless": true}}
 *
 *   1:1: Invalid strong marker `!`: use either `'consistent'`, `'*'`, or `'_'`
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = strongMarker;

/* Map of valid markers. */
var MARKERS = {
  '*': true,
  '_': true,
  'null': true
};

/**
 * Warn when a `strong` node has an incorrect marker.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string?} [preferred='consistent'] - Preferred
 *   marker, either `"*"` or `"_"`, or `"consistent"`.
 */
function strongMarker(ast, file, preferred) {
  preferred = typeof preferred !== 'string' || preferred === 'consistent' ? null : preferred;

  if (MARKERS[preferred] !== true) {
    file.fail('Invalid strong marker `' + preferred + '`: use either `\'consistent\'`, `\'*\'`, or `\'_\'`');
  }

  visit(ast, 'strong', function (node) {
    var marker = file.toString().charAt(position.start(node).offset);

    if (position.generated(node)) {
      return;
    }

    if (preferred) {
      if (marker !== preferred) {
        file.message('Strong should use `' + preferred + '` as a marker', node);
      }
    } else {
      preferred = marker;
    }
  });
}
