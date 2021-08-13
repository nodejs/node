/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-duplicate-definitions
 * @fileoverview
 *   Warn when duplicate definitions are found.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   [foo]: bar
 *   [baz]: qux
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   [foo]: bar
 *   [foo]: qux
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   2:1-2:11: Do not use definitions with the same identifier (1:1)
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {pointStart} from 'unist-util-position'
import {generated} from 'unist-util-generated'
import {stringifyPosition} from 'unist-util-stringify-position'
import {visit} from 'unist-util-visit'

const remarkLintNoDuplicateDefinitions = lintRule(
  'remark-lint:no-duplicate-definitions',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    /** @type {Record<string, string>} */
    const map = Object.create(null)

    visit(tree, (node) => {
      if (
        (node.type === 'definition' || node.type === 'footnoteDefinition') &&
        !generated(node)
      ) {
        const identifier = node.identifier
        const duplicate = map[identifier]

        if (duplicate) {
          file.message(
            'Do not use definitions with the same identifier (' +
              duplicate +
              ')',
            node
          )
        }

        map[identifier] = stringifyPosition(pointStart(node))
      }
    })
  }
)

export default remarkLintNoDuplicateDefinitions
