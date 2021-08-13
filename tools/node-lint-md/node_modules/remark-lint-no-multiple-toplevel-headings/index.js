/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-multiple-toplevel-headings
 * @fileoverview
 *   Warn when multiple top level headings are used.
 *
 *   Options: `number`, default: `1`.
 *
 * @example
 *   {"name": "ok.md", "setting": 1}
 *
 *   # Foo
 *
 *   ## Bar
 *
 * @example
 *   {"name": "not-ok.md", "setting": 1, "label": "input"}
 *
 *   # Foo
 *
 *   # Bar
 *
 * @example
 *   {"name": "not-ok.md", "setting": 1, "label": "output"}
 *
 *   3:1-3:6: Don’t use multiple top level headings (1:1)
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').Heading} Heading
 * @typedef {Heading['depth']} Options
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart} from 'unist-util-position'
import {generated} from 'unist-util-generated'
import {stringifyPosition} from 'unist-util-stringify-position'

const remarkLintNoMultipleToplevelHeadings = lintRule(
  'remark-lint:no-multiple-toplevel-headings',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 1) => {
    /** @type {string|undefined} */
    let duplicate

    visit(tree, 'heading', (node) => {
      if (!generated(node) && node.depth === option) {
        if (duplicate) {
          file.message(
            'Don’t use multiple top level headings (' + duplicate + ')',
            node
          )
        } else {
          duplicate = stringifyPosition(pointStart(node))
        }
      }
    })
  }
)

export default remarkLintNoMultipleToplevelHeadings
