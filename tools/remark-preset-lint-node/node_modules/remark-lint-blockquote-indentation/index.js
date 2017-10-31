/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module blockquote-indentation
 * @fileoverview
 *   Warn when blockquotes are indented too much or too little.
 *
 *   Options: `number` or `'consistent'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used indentation and will warn when
 *   other blockquotes use a different indentation.
 *
 * @example {"name": "valid.md", "setting": 4}
 *
 *   >   Hello
 *
 *   Paragraph.
 *
 *   >   World
 *
 * @example {"name": "valid.md", "setting": 2}
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

var rule = require('unified-lint-rule');
var plural = require('plur');
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var generated = require('unist-util-generated');
var toString = require('mdast-util-to-string');

module.exports = rule('remark-lint:blockquote-indentation', blockquoteIndentation);

function blockquoteIndentation(tree, file, preferred) {
  preferred = isNaN(preferred) || typeof preferred !== 'number' ? null : preferred;

  visit(tree, 'blockquote', visitor);

  function visitor(node) {
    var indent;
    var diff;
    var word;

    if (generated(node) || node.children.length === 0) {
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
  }
}

function check(node) {
  var head = node.children[0];
  var indentation = position.start(head).column - position.start(node).column;
  var padding = toString(head).match(/^ +/);

  if (padding) {
    indentation += padding[0].length;
  }

  return indentation;
}
