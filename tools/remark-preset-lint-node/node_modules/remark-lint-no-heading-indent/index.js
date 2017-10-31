/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-heading-indent
 * @fileoverview
 *   Warn when a heading is indented.
 *
 * @example {"name": "valid.md"}
 *
 *   #·Hello world
 *
 *   Foo
 *   -----
 *
 *   #·Hello world·#
 *
 *   Bar
 *   =====
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   ···# Hello world
 *
 *   ·Foo
 *   -----
 *
 *   ·# Hello world #
 *
 *   ···Bar
 *   =====
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:4: Remove 3 spaces before this heading
 *   3:2: Remove 1 space before this heading
 *   6:2: Remove 1 space before this heading
 *   8:4: Remove 3 spaces before this heading
 */

'use strict';

var rule = require('unified-lint-rule');
var plural = require('plur');
var visit = require('unist-util-visit');
var position = require('unist-util-position');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:no-heading-indent', noHeadingIndent);

var start = position.start;

function noHeadingIndent(ast, file) {
  var contents = file.toString();
  var length = contents.length;

  visit(ast, 'heading', visitor);

  function visitor(node) {
    var initial = start(node);
    var begin = initial.offset;
    var index = begin - 1;
    var character;
    var diff;

    if (generated(node)) {
      return;
    }

    while (++index < length) {
      character = contents.charAt(index);

      if (character !== ' ' && character !== '\t') {
        break;
      }
    }

    diff = index - begin;

    if (diff) {
      file.message(
        'Remove ' + diff + ' ' + plural('space', diff) +
        ' before this heading',
        {
          line: initial.line,
          column: initial.column + diff
        }
      );
    }
  }
}
