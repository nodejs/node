/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-inline-padding
 * @fileoverview
 *   Warn when phrasing content is padded with spaces between their markers and
 *   content.
 *
 *   Warns for emphasis, strong, delete, image, and link.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   Alpha [bravo](http://echo.fox/trot)
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Alpha [ bravo ](http://echo.fox/trot)
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:7-1:38: Don’t pad `link` with inner spaces
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {generated} from 'unist-util-generated'
import {toString} from 'mdast-util-to-string'

const remarkLintNoInlinePadding = lintRule(
  'remark-lint:no-inline-padding',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    // Note: `emphasis`, `strong`, `delete` (GFM) can’t have padding anymore
    // since CM.
    visit(tree, (node) => {
      if (
        (node.type === 'link' || node.type === 'linkReference') &&
        !generated(node)
      ) {
        const value = toString(node)

        if (value.charAt(0) === ' ' || value.charAt(value.length - 1) === ' ') {
          file.message('Don’t pad `' + node.type + '` with inner spaces', node)
        }
      }
    })
  }
)

export default remarkLintNoInlinePadding
