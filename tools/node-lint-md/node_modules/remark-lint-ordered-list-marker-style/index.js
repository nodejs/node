/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module ordered-list-marker-style
 * @fileoverview
 *   Warn when the list item marker style of ordered lists violate a given style.
 *
 *   Options: `'consistent'`, `'.'`, or `')'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used list style and warns when subsequent
 *   lists use different styles.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   1.  Foo
 *
 *
 *   1.  Bar
 *
 *   Unordered lists are not affected by this rule.
 *
 *   * Foo
 *
 * @example
 *   {"name": "ok.md", "setting": "."}
 *
 *   1.  Foo
 *
 *   2.  Bar
 *
 * @example
 *   {"name": "ok.md", "setting": ")"}
 *
 *   1)  Foo
 *
 *   2)  Bar
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   1.  Foo
 *
 *   2)  Bar
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   3:1-3:8: Marker style should be `.`
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "setting": "💩", "positionless": true}
 *
 *   1:1: Incorrect ordered list item marker style `💩`: use either `'.'` or `')'`
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {'.'|')'} Marker
 * @typedef {'consistent'|Marker} Options
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart} from 'unist-util-position'
import {generated} from 'unist-util-generated'

const remarkLintOrderedListMarkerStyle = lintRule(
  'remark-lint:ordered-list-marker-style',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    const value = String(file)

    if (option !== 'consistent' && option !== '.' && option !== ')') {
      file.fail(
        'Incorrect ordered list item marker style `' +
          option +
          "`: use either `'.'` or `')'`"
      )
    }

    visit(tree, 'list', (node) => {
      let index = -1

      if (!node.ordered) return

      while (++index < node.children.length) {
        const child = node.children[index]

        if (!generated(child)) {
          const marker = /** @type {Marker} */ (
            value
              .slice(
                pointStart(child).offset,
                pointStart(child.children[0]).offset
              )
              .replace(/\s|\d/g, '')
              .replace(/\[[x ]?]\s*$/i, '')
          )

          if (option === 'consistent') {
            option = marker
          } else if (marker !== option) {
            file.message('Marker style should be `' + option + '`', child)
          }
        }
      }
    })
  }
)

export default remarkLintOrderedListMarkerStyle
