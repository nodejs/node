/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module hard-break-spaces
 * @fileoverview
 *   Warn when too many spaces are used to create a hard break.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   Lorem ipsum··
 *   dolor sit amet
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Lorem ipsum···
 *   dolor sit amet.
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:12-2:1: Use two spaces for hard line breaks
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart, pointEnd} from 'unist-util-position'
import {generated} from 'unist-util-generated'

const remarkLintHardBreakSpaces = lintRule(
  'remark-lint:hard-break-spaces',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    const value = String(file)

    visit(tree, 'break', (node) => {
      if (!generated(node)) {
        const slice = value
          .slice(pointStart(node).offset, pointEnd(node).offset)
          .split('\n', 1)[0]
          .replace(/\r$/, '')

        if (slice.length > 2) {
          file.message('Use two spaces for hard line breaks', node)
        }
      }
    })
  }
)

export default remarkLintHardBreakSpaces
