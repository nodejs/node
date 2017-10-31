/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module first-heading-level
 * @fileoverview
 *   Warn when the first heading has a level other than a specified value.
 *
 *   Options: `number`, default: `1`.
 *
 * @example {"name": "valid.md"}
 *
 *   # The default is to expect a level one heading
 *
 * @example {"name": "valid-html.md"}
 *
 *   <h1>An HTML heading is also seen by this rule.</h1>
 *
 * @example {"name": "valid-delayed.md"}
 *
 *   You can use markdown content before the heading.
 *
 *   <div>Or non-heading HTML</div>
 *
 *   <h1>So the first heading, be it HTML or markdown, is checked</h1>
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   ## Bravo
 *
 *   Paragraph.
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-1:9: First heading level should be `1`
 *
 * @example {"name": "invalid-html.md", "label": "input"}
 *
 *   <h2>Charlie</h2>
 *
 *   Paragraph.
 *
 * @example {"name": "invalid-html.md", "label": "output"}
 *
 *   1:1-1:17: First heading level should be `1`
 *
 * @example {"name": "valid.md", "setting": 2}
 *
 *   ## Delta
 *
 *   Paragraph.
 *
 * @example {"name": "valid-html.md", "setting": 2}
 *
 *   <h2>Echo</h2>
 *
 *   Paragraph.
 *
 * @example {"name": "invalid.md", "setting": 2, "label": "input"}
 *
 *   # Foxtrot
 *
 *   Paragraph.
 *
 * @example {"name": "invalid.md", "setting": 2, "label": "output"}
 *
 *   1:1-1:10: First heading level should be `2`
 *
 * @example {"name": "invalid-html.md", "setting": 2, "label": "input"}
 *
 *   <h1>Golf</h1>
 *
 *   Paragraph.
 *
 * @example {"name": "invalid-html.md", "setting": 2, "label": "output"}
 *
 *   1:1-1:14: First heading level should be `2`
 */

'use strict';

var rule = require('unified-lint-rule');
var visit = require('unist-util-visit');
var generated = require('unist-util-generated');

module.exports = rule('remark-lint:first-heading-level', firstHeadingLevel);

var re = /<h([1-6])/;

function firstHeadingLevel(tree, file, preferred) {
  var style = preferred && preferred !== true ? preferred : 1;

  visit(tree, visitor);

  function visitor(node) {
    var depth;

    if (generated(node)) {
      return;
    }

    if (node.type === 'heading') {
      depth = node.depth;
    } else if (node.type === 'html') {
      depth = infer(node);
    }

    if (depth !== undefined) {
      if (depth !== style) {
        file.message('First heading level should be `' + style + '`', node);
      }

      return false;
    }
  }
}

function infer(node) {
  var results = node.value.match(re);
  return results ? Number(results[1]) : undefined;
}
