/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module emphasis-marker
 * @fileoverview
 *   Warn for violating emphasis markers.
 *
 *   Options: `string`, either `'consistent'`, `'*'`, or `'_'`,
 *   default: `'consistent'`.
 *
 *   The default value, `consistent`, detects the first used emphasis
 *   style, and will warn when a subsequent emphasis uses a different
 *   style.
 *
 * @example {"setting": "*", "name": "valid.md"}
 *
 *   *foo*
 *
 * @example {"setting": "*", "name": "invalid.md", "label": "input"}
 *
 *   _foo_
 *
 * @example {"setting": "*", "name": "invalid.md", "label": "output"}
 *
 *   1:1-1:6: Emphasis should use `*` as a marker
 *
 * @example {"setting": "_", "name": "valid.md"}
 *
 *   _foo_
 *
 * @example {"setting": "_", "name": "invalid.md", "label": "input"}
 *
 *   *foo*
 *
 * @example {"setting": "_", "name": "invalid.md", "label": "output"}
 *
 *   1:1-1:6: Emphasis should use `_` as a marker
 *
 * @example {"setting": "consistent", "name": "invalid.md", "label": "input"}
 *
 *   <!-- This is never valid -->
 *
 *   *foo*
 *   _bar_
 *
 * @example {"setting": "consistent", "name": "invalid.md", "label": "output"}
 *
 *   4:1-4:6: Emphasis should use `*` as a marker
 *
 * @example {"setting": "invalid", "name": "invalid.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Invalid emphasis marker `invalid`: use either `'consistent'`, `'*'`, or `'_'`
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = emphasisMarker;

/* Map of valid markers. */
var MARKERS = {
  '*': true,
  '_': true,
  'null': true
};

/**
 * Warn when an `emphasis` node has an incorrect marker.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string?} [preferred='consistent'] - Preferred
 *   marker, either `'*'` or `'_'`, or `'consistent'`.
 */
function emphasisMarker(ast, file, preferred) {
  preferred = typeof preferred !== 'string' || preferred === 'consistent' ? null : preferred;

  if (MARKERS[preferred] !== true) {
    file.fail('Invalid emphasis marker `' + preferred + '`: use either `\'consistent\'`, `\'*\'`, or `\'_\'`');
  }

  visit(ast, 'emphasis', function (node) {
    var marker = file.toString().charAt(position.start(node).offset);

    if (position.generated(node)) {
      return;
    }

    if (preferred) {
      if (marker !== preferred) {
        file.message('Emphasis should use `' + preferred + '` as a marker', node);
      }
    } else {
      preferred = marker;
    }
  });
}
