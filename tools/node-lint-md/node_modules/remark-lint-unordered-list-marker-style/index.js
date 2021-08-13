/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module unordered-list-marker-style
 * @fileoverview
 *   Warn when the list item marker style of unordered lists violate a given
 *   style.
 *
 *   Options: `'consistent'`, `'-'`, `'*'`, or `'+'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used list style and warns when subsequent
 *   lists use different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats unordered lists using `-` (hyphen-minus) by default.
 *   Pass
 *   [`bullet: '*'` or `bullet: '+'`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsbullet)
 *   to use `*` (asterisk) or `+` (plus sign) instead.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   By default (`'consistent'`), if the file uses only one marker,
 *   that’s OK.
 *
 *   * Foo
 *   * Bar
 *   * Baz
 *
 *   Ordered lists are not affected.
 *
 *   1. Foo
 *   2. Bar
 *   3. Baz
 *
 * @example
 *   {"name": "ok.md", "setting": "*"}
 *
 *   * Foo
 *
 * @example
 *   {"name": "ok.md", "setting": "-"}
 *
 *   - Foo
 *
 * @example
 *   {"name": "ok.md", "setting": "+"}
 *
 *   + Foo
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   * Foo
 *   - Bar
 *   + Baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   2:1-2:6: Marker style should be `*`
 *   3:1-3:6: Marker style should be `*`
 *
 * @example
 *   {"name": "not-ok.md", "label": "output", "setting": "💩", "positionless": true}
 *
 *   1:1: Incorrect unordered list item marker style `💩`: use either `'-'`, `'*'`, or `'+'`
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {'-'|'*'|'+'} Marker
 * @typedef {'consistent'|Marker} Options
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart} from 'unist-util-position'
import {generated} from 'unist-util-generated'

const markers = new Set(['-', '*', '+'])

const remarkLintUnorderedListMarkerStyle = lintRule(
  'remark-lint:unordered-list-marker-style',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    const value = String(file)

    if (option !== 'consistent' && !markers.has(option)) {
      file.fail(
        'Incorrect unordered list item marker style `' +
          option +
          "`: use either `'-'`, `'*'`, or `'+'`"
      )
    }

    visit(tree, 'list', (node) => {
      if (node.ordered) return

      let index = -1

      while (++index < node.children.length) {
        const child = node.children[index]

        if (!generated(child)) {
          const marker = /** @type {Marker} */ (
            value
              .slice(
                pointStart(child).offset,
                pointStart(child.children[0]).offset
              )
              .replace(/\[[x ]?]\s*$/i, '')
              .replace(/\s/g, '')
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

export default remarkLintUnorderedListMarkerStyle
