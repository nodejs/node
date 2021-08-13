/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-heading-content-indent
 * @fileoverview
 *   Warn when content of headings is indented.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   removes all unneeded padding around content in headings.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   #·Foo
 *
 *   ## Bar·##
 *
 *     ##·Baz
 *
 *   Setext headings are not affected.
 *
 *   Baz
 *   ===
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   #··Foo
 *
 *   ## Bar··##
 *
 *     ##··Baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:4: Remove 1 space before this heading’s content
 *   3:7: Remove 1 space after this heading’s content
 *   5:7: Remove 1 space before this heading’s content
 *
 * @example
 *   {"name": "empty-heading.md"}
 *
 *   #··
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {headingStyle} from 'mdast-util-heading-style'
import plural from 'pluralize'
import {pointStart, pointEnd} from 'unist-util-position'
import {generated} from 'unist-util-generated'

const remarkLintNoHeadingContentIndent = lintRule(
  'remark-lint:no-heading-content-indent',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit(tree, 'heading', (node) => {
      if (generated(node)) {
        return
      }

      const type = headingStyle(node, 'atx')

      if (type === 'atx' || type === 'atx-closed') {
        const head = pointStart(node.children[0]).column

        // Ignore empty headings.
        if (!head) {
          return
        }

        const diff = head - pointStart(node).column - 1 - node.depth

        if (diff) {
          file.message(
            'Remove ' +
              Math.abs(diff) +
              ' ' +
              plural('space', Math.abs(diff)) +
              ' before this heading’s content',
            pointStart(node.children[0])
          )
        }
      }

      // Closed ATX headings always must have a space between their content and
      // the final hashes, thus, there is no `add x spaces`.
      if (type === 'atx-closed') {
        const final = pointEnd(node.children[node.children.length - 1])
        const diff = pointEnd(node).column - final.column - 1 - node.depth

        if (diff) {
          file.message(
            'Remove ' +
              diff +
              ' ' +
              plural('space', diff) +
              ' after this heading’s content',
            final
          )
        }
      }
    })
  }
)

export default remarkLintNoHeadingContentIndent
