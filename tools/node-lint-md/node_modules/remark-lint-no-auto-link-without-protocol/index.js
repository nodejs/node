/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-auto-link-without-protocol
 * @fileoverview
 *   Warn for autolinks without protocol.
 *   Autolinks are URLs enclosed in `<` (less than) and `>` (greater than)
 *   characters.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   adds a protocol where needed.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   <http://www.example.com>
 *   <mailto:foo@bar.com>
 *
 *   Most Markdown vendors don’t recognize the following as a link:
 *   <www.example.com>
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   <foo@bar.com>
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:14: All automatic links must start with a protocol
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart, pointEnd} from 'unist-util-position'
import {generated} from 'unist-util-generated'
import {toString} from 'mdast-util-to-string'

// Protocol expression.
// See: <https://en.wikipedia.org/wiki/URI_scheme#Generic_syntax>.
const protocol = /^[a-z][a-z+.-]+:\/?/i

const remarkLintNoAutoLinkWithoutProtocol = lintRule(
  'remark-lint:no-auto-link-without-protocol',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit(tree, 'link', (node) => {
      if (
        !generated(node) &&
        pointStart(node).column === pointStart(node.children[0]).column - 1 &&
        pointEnd(node).column ===
          pointEnd(node.children[node.children.length - 1]).column + 1 &&
        !protocol.test(toString(node))
      ) {
        file.message('All automatic links must start with a protocol', node)
      }
    })
  }
)

export default remarkLintNoAutoLinkWithoutProtocol
