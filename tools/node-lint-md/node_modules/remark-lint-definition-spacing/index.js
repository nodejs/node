/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module definition-spacing
 * @fileoverview
 *   Warn when consecutive whitespace is used in a definition.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   [example domain]: http://example.com "Example Domain"
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   [example····domain]: http://example.com "Example Domain"
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:57: Do not use consecutive whitespace in definition labels
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart, pointEnd} from 'unist-util-position'

const label = /^\s*\[((?:\\[\s\S]|[^[\]])+)]/

const remarkLintDefinitionSpacing = lintRule(
  'remark-lint:definition-spacing',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    const value = String(file)

    visit(tree, (node) => {
      if (node.type === 'definition' || node.type === 'footnoteDefinition') {
        const start = pointStart(node).offset
        const end = pointEnd(node).offset

        if (typeof start === 'number' && typeof end === 'number') {
          const match = value.slice(start, end).match(label)

          if (match && /[ \t\n]{2,}/.test(match[1])) {
            file.message(
              'Do not use consecutive whitespace in definition labels',
              node
            )
          }
        }
      }
    })
  }
)

export default remarkLintDefinitionSpacing
