/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module ordered-list-marker-value
 * @fileoverview
 *   Warn when the list-item marker values of ordered lists violate a
 *   given style.
 *
 *   Options: `string`, either `'single'`, `'one'`, or `'ordered'`,
 *   default: `'ordered'`.
 *
 *   When set to `'ordered'`, list-item bullets should increment by one,
 *   relative to the starting point.  When set to `'single'`, bullets should
 *   be the same as the relative starting point.  When set to `'one'`, bullets
 *   should always be `1`.
 *
 * @example {"name": "valid.md", "setting": "one"}
 *
 *   1.  Foo
 *   1.  Bar
 *   1.  Baz
 *
 *   Paragraph.
 *
 *   1.  Alpha
 *   1.  Bravo
 *   1.  Charlie
 *
 * @example {"name": "valid.md", "setting": "single"}
 *
 *   1.  Foo
 *   1.  Bar
 *   1.  Baz
 *
 *   Paragraph.
 *
 *   3.  Alpha
 *   3.  Bravo
 *   3.  Charlie
 *
 * @example {"name": "valid.md", "setting": "ordered"}
 *
 *   1.  Foo
 *   2.  Bar
 *   3.  Baz
 *
 *   Paragraph.
 *
 *   3.  Alpha
 *   4.  Bravo
 *   5.  Charlie
 *
 * @example {"name": "invalid.md", "setting": "one", "label": "input"}
 *
 *   1.  Foo
 *   2.  Bar
 *
 * @example {"name": "invalid.md", "setting": "one", "label": "output"}
 *
 *   2:1-2:8: Marker should be `1`, was `2`
 *
 * @example {"name": "invalid.md", "setting": "ordered", "label": "input"}
 *
 *   1.  Foo
 *   1.  Bar
 *
 * @example {"name": "invalid.md", "setting": "ordered", "label": "output"}
 *
 *   2:1-2:8: Marker should be `2`, was `1`
 *
 * @example {"name": "invalid.md", "setting": "invalid", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Invalid ordered list-item marker value `invalid`: use either `'ordered'` or `'one'`
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = orderedListMarkerValue;

/* Methods. */
var start = position.start;

/* Valid styles. */
var STYLES = {
  ordered: true,
  single: true,
  one: true
};

/**
 * Warn when the list-item markers values of ordered lists
 * violate a given style.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string?} [preferred='ordered'] - Ordered list
 *   marker value, either `'one'` or `'ordered'`,
 *   defaulting to the latter.
 */
function orderedListMarkerValue(ast, file, preferred) {
  var contents = file.toString();

  preferred = typeof preferred === 'string' ? preferred : 'ordered';

  if (STYLES[preferred] !== true) {
    file.fail('Invalid ordered list-item marker value `' + preferred + '`: use either `\'ordered\'` or `\'one\'`');
  }

  visit(ast, 'list', function (node) {
    var items = node.children;
    var shouldBe = (preferred === 'one' ? 1 : node.start) || 1;

    /* Ignore unordered lists. */
    if (!node.ordered) {
      return;
    }

    items.forEach(function (item, index) {
      var head = item.children[0];
      var initial = start(item).offset;
      var final = start(head).offset;
      var marker;

      /* Ignore first list item. */
      if (index === 0) {
        return;
      }

      /* Increase the expected line number when in
       * `ordered` mode. */
      if (preferred === 'ordered') {
        shouldBe++;
      }

      /* Ignore generated nodes. */
      if (position.generated(item)) {
        return;
      }

      marker = contents.slice(initial, final).replace(/[\s\.\)]/g, '');

      /* Support checkboxes. */
      marker = Number(marker.replace(/\[[x ]?\]\s*$/i, ''));

      if (marker !== shouldBe) {
        file.message('Marker should be `' + shouldBe + '`, was `' + marker + '`', item);
      }
    });
  });
}
