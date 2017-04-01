/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-heading-content-indent
 * @fileoverview
 *   Warn when a heading’s content is indented.
 *
 * @example {"name": "valid.md"}
 *
 *   <!-- Note: the middle-dots represent spaces -->
 *
 *   #·Foo
 *
 *   ## Bar·##
 *
 *     ##·Baz
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   <!-- Note: the middle-dots represent spaces -->
 *
 *   #··Foo
 *
 *   ## Bar··##
 *
 *     ##··Baz
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   3:4: Remove 1 space before this heading’s content
 *   5:7: Remove 1 space after this heading’s content
 *   7:7: Remove 1 space before this heading’s content
 *
 * @example {"name": "empty-heading.md"}
 *
 *   #··
 *
 * @example {"name": "tight.md", "config":{"pedantic":true}, "label": "input"}
 *
 *   In pedantic mode, headings without spacing can also be detected:
 *
 *   ##No spacing left, too much right··##
 *
 * @example {"name": "tight.md", "label": "output"}
 *
 *   3:3: Add 1 space before this heading’s content
 *   3:34: Remove 1 space after this heading’s content
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var style = require('mdast-util-heading-style');
var plural = require('plur');
var position = require('unist-util-position');

/* Expose. */
module.exports = noHeadingContentIndent;

/* Methods. */
var start = position.start;
var end = position.end;

/**
 * Warn when a (closed) ATX-heading has too much space
 * between the initial hashes and the content, or the
 * content and the final hashes.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noHeadingContentIndent(ast, file) {
  var contents = file.toString();

  visit(ast, 'heading', function (node) {
    var depth = node.depth;
    var children = node.children;
    var type = style(node, 'atx');
    var head;
    var initial;
    var final;
    var diff;
    var word;
    var index;
    var char;

    if (position.generated(node)) {
      return;
    }

    if (type === 'atx' || type === 'atx-closed') {
      initial = start(node);
      index = initial.offset;
      char = contents.charAt(index);

      while (char && char !== '#') {
        index++;
        char = contents.charAt(index);
      }

      /* istanbul ignore if - CR/LF bug: wooorm/remark#195. */
      if (!char) {
        return;
      }

      index = depth + (index - initial.offset);
      head = start(children[0]).column;

      /*
       * Ignore empty headings.
       */

      if (!head) {
        return;
      }

      diff = head - initial.column - 1 - index;

      if (diff) {
        word = diff > 0 ? 'Remove' : 'Add';
        diff = Math.abs(diff);

        file.message(
          word + ' ' + diff + ' ' + plural('space', diff) +
          ' before this heading’s content',
          start(children[0])
        );
      }
    }

    /*
     * Closed ATX-heading always must have a space
     * between their content and the final hashes,
     * thus, there is no `add x spaces`.
     */

    if (type === 'atx-closed') {
      final = end(children[children.length - 1]);
      diff = end(node).column - final.column - 1 - depth;

      if (diff) {
        file.message(
          'Remove ' + diff + ' ' + plural('space', diff) +
          ' after this heading’s content',
          final
        );
      }
    }
  });
}
