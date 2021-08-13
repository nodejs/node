/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module fenced-code-marker
 * @fileoverview
 *   Warn for violating fenced code markers.
 *
 *   Options: `` '`' ``, `'~'`, or `'consistent'`, default: `'consistent'`.
 *
 *   `'consistent'` detects the first used fenced code marker style and warns
 *   when subsequent fenced code blocks use different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats fences using ``'`'`` (grave accent) by default.
 *   Pass
 *   [`fence: '~'`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionsfence)
 *   to use `~` (tilde) instead.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   Indented code blocks are not affected by this rule:
 *
 *       bravo()
 *
 * @example
 *   {"name": "ok.md", "setting": "`"}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 *   ```
 *   charlie()
 *   ```
 *
 * @example
 *   {"name": "ok.md", "setting": "~"}
 *
 *   ~~~alpha
 *   bravo()
 *   ~~~
 *
 *   ~~~
 *   charlie()
 *   ~~~
 *
 * @example
 *   {"name": "not-ok-consistent-tick.md", "label": "input"}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 *   ~~~
 *   charlie()
 *   ~~~
 *
 * @example
 *   {"name": "not-ok-consistent-tick.md", "label": "output"}
 *
 *   5:1-7:4: Fenced code should use `` ` `` as a marker
 *
 * @example
 *   {"name": "not-ok-consistent-tilde.md", "label": "input"}
 *
 *   ~~~alpha
 *   bravo()
 *   ~~~
 *
 *   ```
 *   charlie()
 *   ```
 *
 * @example
 *   {"name": "not-ok-consistent-tilde.md", "label": "output"}
 *
 *   5:1-7:4: Fenced code should use `~` as a marker
 *
 * @example
 *   {"name": "not-ok-incorrect.md", "setting": "💩", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect fenced code marker `💩`: use either `'consistent'`, `` '`' ``, or `'~'`
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {'~'|'`'} Marker
 * @typedef {'consistent'|Marker} Options
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart} from 'unist-util-position'

const remarkLintFencedCodeMarker = lintRule(
  'remark-lint:fenced-code-marker',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    const contents = String(file)

    if (option !== 'consistent' && option !== '~' && option !== '`') {
      file.fail(
        'Incorrect fenced code marker `' +
          option +
          "`: use either `'consistent'`, `` '`' ``, or `'~'`"
      )
    }

    visit(tree, 'code', (node) => {
      const start = pointStart(node).offset

      if (typeof start === 'number') {
        const marker = contents
          .slice(start, start + 4)
          .replace(/^\s+/, '')
          .charAt(0)

        // Ignore unfenced code blocks.
        if (marker === '~' || marker === '`') {
          if (option === 'consistent') {
            option = marker
          } else if (marker !== option) {
            file.message(
              'Fenced code should use `' +
                (option === '~' ? option : '` ` `') +
                '` as a marker',
              node
            )
          }
        }
      }
    })
  }
)

export default remarkLintFencedCodeMarker
