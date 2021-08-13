/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module table-pipes
 * @fileoverview
 *   Warn when table rows are not fenced with pipes.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   creates fenced rows with initial and final pipes by default.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md", "gfm": true}
 *
 *   | A     | B     |
 *   | ----- | ----- |
 *   | Alpha | Bravo |
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "gfm": true}
 *
 *   A     | B
 *   ----- | -----
 *   Alpha | Bravo
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "gfm": true}
 *
 *   1:1: Missing initial pipe in table fence
 *   1:10: Missing final pipe in table fence
 *   3:1: Missing initial pipe in table fence
 *   3:14: Missing final pipe in table fence
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart, pointEnd} from 'unist-util-position'

const reasonStart = 'Missing initial pipe in table fence'
const reasonEnd = 'Missing final pipe in table fence'

const remarkLintTablePipes = lintRule(
  'remark-lint:table-pipes',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    const value = String(file)

    visit(tree, 'table', (node) => {
      let index = -1

      while (++index < node.children.length) {
        const row = node.children[index]
        const start = pointStart(row)
        const end = pointEnd(row)

        if (
          typeof start.offset === 'number' &&
          value.charCodeAt(start.offset) !== 124
        ) {
          file.message(reasonStart, start)
        }

        if (
          typeof end.offset === 'number' &&
          value.charCodeAt(end.offset - 1) !== 124
        ) {
          file.message(reasonEnd, end)
        }
      }
    })
  }
)

export default remarkLintTablePipes
