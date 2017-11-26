/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module heading-style
 * @fileoverview
 *   Warn when a heading does not conform to a given style.
 *
 *   Options: `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`,
 *   default: `'consistent'`.
 *
 *   `'consistent'` detects the first used heading style and warns when
 *   subsequent headings use different styles.
 *
 * @example {"name": "valid.md", "setting": "atx"}
 *
 *   # Alpha
 *
 *   ## Bravo
 *
 *   ### Charlie
 *
 * @example {"name": "valid.md", "setting": "atx-closed"}
 *
 *   # Delta ##
 *
 *   ## Echo ##
 *
 *   ### Foxtrot ###
 *
 * @example {"name": "valid.md", "setting": "setext"}
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
 *   Juliett
 *   =======
 *
 *   ## Kilo
 *
 *   ### Lima ###
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   4:1-4:8: Headings should use setext
 *   6:1-6:13: Headings should use setext
 */

'use strict';

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var style = require('mdast-util-heading-style');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:heading-style', headingStyle);

var TYPES = ['atx', 'atx-closed', 'setext'];

function headingStyle(ast, file, preferred) {
  preferred = TYPES.indexOf(preferred) === -1 ? null : preferred;

  visit(ast, 'heading', visitor);

  function visitor(node) {
    if (generated(node)) {
      return;
    }

    if (preferred) {
      if (style(node, preferred) !== preferred) {
        file.message('Headings should use ' + preferred, node);
      }
    } else {
      preferred = style(node, preferred);
    }
  }
}
