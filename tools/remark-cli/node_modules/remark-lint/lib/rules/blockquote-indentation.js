/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module blockquote-indentation
 * @fileoverview
 *   Warn when blockquotes are either indented too much or too little.
 *
 *   Options: `number`, default: `'consistent'`.
 *
 *   The default value, `consistent`, detects the first used indentation
 *   and will warn when other blockquotes use a different indentation.
 *
 * @example {"name": "valid.md", "setting": 4}
 *
 *   <!--This file is also valid by default-->
 *
 *   >   Hello
 *
 *   Paragraph.
 *
 *   >   World
 *
 * @example {"name": "valid.md", "setting": 2}
 *
 *   <!--This file is also valid by default-->
 *
 *   > Hello
 *
 *   Paragraph.
 *
 *   > World
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   >  Hello
 *
 *   Paragraph.
 *
 *   >   World
 *
 *   Paragraph.
 *
 *   > World
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   5:3: Remove 1 space between blockquote and content
 *   9:3: Add 1 space between blockquote and content
 */

'use strict';

/* Expose. */
module.exports = blockquoteIndentation;

/* Dependencies. */
var visit = require('unist-util-visit');
var toString = require('mdast-util-to-string');
var plural = require('plur');
var position = require('unist-util-position');

/**
 * Warn when a blockquote has a too large or too small
 * indentation.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {number?} [preferred='consistent'] - Preferred
 *   indentation between a blockquote and its content.
 *   When not a number, defaults to the first found
 *   indentation.
 */
function blockquoteIndentation(ast, file, preferred) {
  preferred = isNaN(preferred) || typeof preferred !== 'number' ? null : preferred;

  visit(ast, 'blockquote', function (node) {
    var indent;
    var diff;
    var word;

    if (position.generated(node) || !node.children.length) {
      return;
    }

    if (preferred) {
      indent = check(node);
      diff = preferred - indent;
      word = diff > 0 ? 'Add' : 'Remove';

      diff = Math.abs(diff);

      if (diff !== 0) {
        file.message(
          word + ' ' + diff + ' ' + plural('space', diff) +
          ' between blockquote and content',
          position.start(node.children[0])
        );
      }
    } else {
      preferred = check(node);
    }
  });
}

/**
 * Get the indent of a blockquote.
 *
 * @param {Node} node - Node to test.
 * @return {number} - Indentation.
 */
function check(node) {
  var head = node.children[0];
  var indentation = position.start(head).column - position.start(node).column;
  var padding = toString(head).match(/^ +/);

  if (padding) {
    indentation += padding[0].length;
  }

  return indentation;
}
