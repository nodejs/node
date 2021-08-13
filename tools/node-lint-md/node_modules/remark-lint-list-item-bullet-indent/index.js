/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module list-item-bullet-indent
 * @fileoverview
 *   Warn when list item bullets are indented.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   removes all indentation before bullets.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   Paragraph.
 *
 *   * List item
 *   * List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Paragraph.
 *
 *   ·* List item
 *   ·* List item
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   3:2: Incorrect indentation before bullet: remove 1 space
 *   4:2: Incorrect indentation before bullet: remove 1 space
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import plural from 'pluralize'
import {visit} from 'unist-util-visit'

const remarkLintListItemBulletIndent = lintRule(
  'remark-lint:list-item-bullet-indent',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit(tree, 'list', (list, _, grandparent) => {
      let index = -1

      while (++index < list.children.length) {
        const item = list.children[index]

        if (
          grandparent &&
          grandparent.type === 'root' &&
          grandparent.position &&
          typeof grandparent.position.start.column === 'number' &&
          item.position &&
          typeof item.position.start.column === 'number'
        ) {
          const indent =
            item.position.start.column - grandparent.position.start.column

          if (indent) {
            file.message(
              'Incorrect indentation before bullet: remove ' +
                indent +
                ' ' +
                plural('space', indent),
              item.position.start
            )
          }
        }
      }
    })
  }
)

export default remarkLintListItemBulletIndent
