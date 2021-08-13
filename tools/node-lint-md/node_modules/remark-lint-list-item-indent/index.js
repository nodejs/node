/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module list-item-indent
 * @fileoverview
 *   Warn when the spacing between a list item’s bullet and its content violates
 *   a given style.
 *
 *   Options: `'tab-size'`, `'mixed'`, or `'space'`, default: `'tab-size'`.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   uses `'tab-size'` (named `'tab'` there) by default to ensure Markdown is
 *   seen the same way across vendors.
 *   This can be configured with the
 *   [`listItemIndent`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionslistitemindent)
 *   option.
 *   This rule’s `'space'` option is named `'1'` there.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   *···List
 *   ····item.
 *
 *   Paragraph.
 *
 *   11.·List
 *   ····item.
 *
 *   Paragraph.
 *
 *   *···List
 *   ····item.
 *
 *   *···List
 *   ····item.
 *
 * @example
 *   {"name": "ok.md", "setting": "mixed"}
 *
 *   *·List item.
 *
 *   Paragraph.
 *
 *   11.·List item
 *
 *   Paragraph.
 *
 *   *···List
 *   ····item.
 *
 *   *···List
 *   ····item.
 *
 * @example
 *   {"name": "ok.md", "setting": "space"}
 *
 *   *·List item.
 *
 *   Paragraph.
 *
 *   11.·List item
 *
 *   Paragraph.
 *
 *   *·List
 *   ··item.
 *
 *   *·List
 *   ··item.
 *
 * @example
 *   {"name": "not-ok.md", "setting": "space", "label": "input"}
 *
 *   *···List
 *   ····item.
 *
 * @example
 *   {"name": "not-ok.md", "setting": "space", "label": "output"}
 *
 *    1:5: Incorrect list-item indent: remove 2 spaces
 *
 * @example
 *   {"name": "not-ok.md", "setting": "tab-size", "label": "input"}
 *
 *   *·List
 *   ··item.
 *
 * @example
 *   {"name": "not-ok.md", "setting": "tab-size", "label": "output"}
 *
 *    1:3: Incorrect list-item indent: add 2 spaces
 *
 * @example
 *   {"name": "not-ok.md", "setting": "mixed", "label": "input"}
 *
 *   *···List item.
 *
 * @example
 *   {"name": "not-ok.md", "setting": "mixed", "label": "output"}
 *
 *    1:5: Incorrect list-item indent: remove 2 spaces
 *
 * @example
 *   {"name": "not-ok.md", "setting": "💩", "label": "output", "positionless": true}
 *
 *    1:1: Incorrect list-item indent style `💩`: use either `'tab-size'`, `'space'`, or `'mixed'`
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {'tab-size'|'space'|'mixed'} Options
 */

import {lintRule} from 'unified-lint-rule'
import plural from 'pluralize'
import {visit} from 'unist-util-visit'
import {pointStart} from 'unist-util-position'
import {generated} from 'unist-util-generated'

const remarkLintListItemIndent = lintRule(
  'remark-lint:list-item-indent',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'tab-size') => {
    const value = String(file)

    if (option !== 'tab-size' && option !== 'space' && option !== 'mixed') {
      file.fail(
        'Incorrect list-item indent style `' +
          option +
          "`: use either `'tab-size'`, `'space'`, or `'mixed'`"
      )
    }

    visit(tree, 'list', (node) => {
      if (generated(node)) return

      const spread = node.spread
      let index = -1

      while (++index < node.children.length) {
        const item = node.children[index]
        const head = item.children[0]
        const final = pointStart(head)

        const marker = value
          .slice(pointStart(item).offset, final.offset)
          .replace(/\[[x ]?]\s*$/i, '')

        const bulletSize = marker.replace(/\s+$/, '').length

        const style =
          option === 'tab-size' || (option === 'mixed' && spread)
            ? Math.ceil(bulletSize / 4) * 4
            : bulletSize + 1

        if (marker.length !== style) {
          const diff = style - marker.length
          const abs = Math.abs(diff)

          file.message(
            'Incorrect list-item indent: ' +
              (diff > 0 ? 'add' : 'remove') +
              ' ' +
              abs +
              ' ' +
              plural('space', abs),
            final
          )
        }
      }
    })
  }
)

export default remarkLintListItemIndent
