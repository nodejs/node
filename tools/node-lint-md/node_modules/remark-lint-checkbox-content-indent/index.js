/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module checkbox-content-indent
 * @fileoverview
 *   Warn when list item checkboxes are followed by too much whitespace.
 *
 * @example
 *   {"name": "ok.md", "gfm": true}
 *
 *   - [ ] List item
 *   +  [x] List Item
 *   *   [X] List item
 *   -    [ ] List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "gfm": true}
 *
 *   - [ ] List item
 *   + [x]  List item
 *   * [X]   List item
 *   - [ ]    List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "gfm": true}
 *
 *   2:7-2:8: Checkboxes should be followed by a single character
 *   3:7-3:9: Checkboxes should be followed by a single character
 *   4:7-4:10: Checkboxes should be followed by a single character
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {location} from 'vfile-location'
import {visit} from 'unist-util-visit'
import {pointStart} from 'unist-util-position'

const remarkLintCheckboxContentIndent = lintRule(
  'remark-lint:checkbox-content-indent',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    const value = String(file)
    const loc = location(file)

    visit(tree, 'listItem', (node) => {
      const head = node.children[0]
      const point = pointStart(head)

      // Exit early for items without checkbox.
      // A list item cannot be checked and empty, according to GFM.
      if (
        typeof node.checked !== 'boolean' ||
        !head ||
        typeof point.offset !== 'number'
      ) {
        return
      }

      // Assume we start with a checkbox, because well, `checked` is set.
      const match = /\[([\t xX])]/.exec(
        value.slice(point.offset - 4, point.offset + 1)
      )

      // Failsafe to make sure we don‘t crash if there actually isn’t a checkbox.
      /* c8 ignore next */
      if (!match) return

      // Move past checkbox.
      const initial = point.offset
      let final = initial

      while (/[\t ]/.test(value.charAt(final))) final++

      if (final - initial > 0) {
        file.message('Checkboxes should be followed by a single character', {
          start: loc.toPoint(initial),
          end: loc.toPoint(final)
        })
      }
    })
  }
)

export default remarkLintCheckboxContentIndent
