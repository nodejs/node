/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module heading-style
 * @fileoverview
 *   Warn when a heading does not conform to a given style.
 *
 *   Options: `string`, either `'consistent'`, `'atx'`, `'atx-closed'`,
 *   or `'setext'`, default: `'consistent'`.
 *
 *   The default value, `consistent`, detects the first used heading
 *   style, and will warn when a subsequent heading uses a different
 *   style.
 *
 * @example {"name": "valid.md", "setting": "atx"}
 *
 *   <!--Also valid when `consistent`-->
 *
 *   # Alpha
 *
 *   ## Bravo
 *
 *   ### Charlie
 *
 * @example {"name": "valid.md", "setting": "atx-closed"}
 *
 *   <!--Also valid when `consistent`-->
 *
 *   # Delta ##
 *
 *   ## Echo ##
 *
 *   ### Foxtrot ###
 *
 * @example {"name": "valid.md", "setting": "setext"}
 *
 *   <!--Also valid when `consistent`-->
 *
 *   Golf
 *   ====
 *
 *   Hotel
 *   -----
 *
 *   ### India
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   <!--Always invalid.-->
 *
 *   Juliett
 *   =======
 *
 *   ## Kilo
 *
 *   ### Lima ###
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   6:1-6:8: Headings should use setext
 *   8:1-8:13: Headings should use setext
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var visit = require('unist-util-visit');
var style = require('mdast-util-heading-style');
var position = require('unist-util-position');

/* Expose. */
module.exports = headingStyle;

/* Types. */
var TYPES = ['atx', 'atx-closed', 'setext'];

/**
 * Warn when a heading does not conform to a given style.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string} [preferred='consistent'] - Preferred
 *   style, one of `atx`, `atx-closed`, or `setext`.
 *   Other values default to `'consistent'`, which will
 *   detect the first used style.
 * @param {Function} done - Callback.
 */
function headingStyle(ast, file, preferred) {
  preferred = TYPES.indexOf(preferred) === -1 ? null : preferred;

  visit(ast, 'heading', function (node) {
    if (position.generated(node)) {
      return;
    }

    if (preferred) {
      if (style(node, preferred) !== preferred) {
        file.message('Headings should use ' + preferred, node);
      }
    } else {
      preferred = style(node, preferred);
    }
  });
}
