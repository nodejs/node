/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-consecutive-blank-lines
 * @fileoverview
 *   Warn for too many consecutive blank lines.
 *   Knows about the extra line needed between a list and indented code, and two
 *   lists.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   always uses one blank line between blocks if possible, or two lines when
 *   needed.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   Foo…
 *   ␊
 *   …Bar.
 *
 * @example
 *   {"name": "empty-document.md"}
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Foo…
 *   ␊
 *   ␊
 *   …Bar
 *   ␊
 *   ␊
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   4:1: Remove 1 line before node
 *   4:5: Remove 2 lines after node
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('unist').Point} Point
 */

import {lintRule} from 'unified-lint-rule'
import plural from 'pluralize'
import {visit} from 'unist-util-visit'
import {pointStart, pointEnd} from 'unist-util-position'
import {generated} from 'unist-util-generated'

const remarkLintNoConsecutiveBlankLines = lintRule(
  'remark-lint:no-consecutive-blank-lines',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit(tree, (node) => {
      if (!generated(node) && 'children' in node) {
        const head = node.children[0]

        if (head && !generated(head)) {
          // Compare parent and first child.
          compare(pointStart(node), pointStart(head), 0)

          // Compare between each child.
          let index = -1

          while (++index < node.children.length) {
            const previous = node.children[index - 1]
            const child = node.children[index]

            if (previous && !generated(previous) && !generated(child)) {
              compare(pointEnd(previous), pointStart(child), 2)
            }
          }

          const tail = node.children[node.children.length - 1]

          // Compare parent and last child.
          if (tail !== head && !generated(tail)) {
            compare(pointEnd(node), pointEnd(tail), 1)
          }
        }
      }
    })

    /**
     * Compare the difference between `start` and `end`, and warn when that
     * difference exceeds `max`.
     *
     * @param {Point} start
     * @param {Point} end
     * @param {0|1|2} max
     */
    function compare(start, end, max) {
      const diff = end.line - start.line
      const lines = Math.abs(diff) - max

      if (lines > 0) {
        file.message(
          'Remove ' +
            lines +
            ' ' +
            plural('line', Math.abs(lines)) +
            ' ' +
            (diff > 0 ? 'before' : 'after') +
            ' node',
          end
        )
      }
    }
  }
)

export default remarkLintNoConsecutiveBlankLines
