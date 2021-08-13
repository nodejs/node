/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-heading-indent
 * @fileoverview
 *   Warn when a heading is indented.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   removes all unneeded indentation before headings.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
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
 * @example
 *   {"name": "not-ok.md", "label": "input"}
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
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:4: Remove 3 spaces before this heading
 *   3:2: Remove 1 space before this heading
 *   6:2: Remove 1 space before this heading
 *   8:4: Remove 3 spaces before this heading
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import plural from 'pluralize'
import {visit} from 'unist-util-visit'
import {pointStart} from 'unist-util-position'
import {generated} from 'unist-util-generated'

const remarkLintNoHeadingIndent = lintRule(
  'remark-lint:no-heading-indent',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit(tree, 'heading', (node, _, parent) => {
      // Note: it’s rather complex to detect what the expected indent is in block
      // quotes and lists, so let’s only do directly in root for now.
      if (generated(node) || (parent && parent.type !== 'root')) {
        return
      }

      const diff = pointStart(node).column - 1

      if (diff) {
        file.message(
          'Remove ' +
            diff +
            ' ' +
            plural('space', diff) +
            ' before this heading',
          pointStart(node)
        )
      }
    })
  }
)

export default remarkLintNoHeadingIndent
