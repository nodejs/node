/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-literal-urls
 * @fileoverview
 *   Warn for literal URLs in text.
 *   URLs are treated as links in some Markdown vendors, but not in others.
 *   To make sure they are always linked, wrap them in `<` (less than) and `>`
 *   (greater than).
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   never creates literal URLs and always uses `<` (less than) and `>`
 *   (greater than).
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   <http://foo.bar/baz>
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "gfm": true}
 *
 *   http://foo.bar/baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "gfm": true}
 *
 *   1:1-1:19: Don’t use literal URLs without angle brackets
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart, pointEnd} from 'unist-util-position'
import {generated} from 'unist-util-generated'
import {toString} from 'mdast-util-to-string'

const remarkLintNoLiteralUrls = lintRule(
  'remark-lint:no-literal-urls',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit(tree, 'link', (node) => {
      const value = toString(node)

      if (
        !generated(node) &&
        pointStart(node).column === pointStart(node.children[0]).column &&
        pointEnd(node).column ===
          pointEnd(node.children[node.children.length - 1]).column &&
        (node.url === 'mailto:' + value || node.url === value)
      ) {
        file.message('Don’t use literal URLs without angle brackets', node)
      }
    })
  }
)

export default remarkLintNoLiteralUrls
