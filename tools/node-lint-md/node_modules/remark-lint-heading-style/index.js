/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module heading-style
 * @fileoverview
 *   Warn when a heading does not conform to a given style.
 *
 *   Options: `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`,
 *   default: `'consistent'`.
 *
 *   `'consistent'` detects the first used heading style and warns when
 *   subsequent headings use different styles.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   formats headings as ATX by default.
 *   This can be configured with the
 *   [`setext`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionssetext)
 *   and
 *   [`closeAtx`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify#optionscloseatx)
 *   options.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md", "setting": "atx"}
 *
 *   # Alpha
 *
 *   ## Bravo
 *
 *   ### Charlie
 *
 * @example
 *   {"name": "ok.md", "setting": "atx-closed"}
 *
 *   # Delta ##
 *
 *   ## Echo ##
 *
 *   ### Foxtrot ###
 *
 * @example
 *   {"name": "ok.md", "setting": "setext"}
 *
 *   Golf
 *   ====
 *
 *   Hotel
 *   -----
 *
 *   ### India
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   Juliett
 *   =======
 *
 *   ## Kilo
 *
 *   ### Lima ###
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   4:1-4:8: Headings should use setext
 *   6:1-6:13: Headings should use setext
 *
 * @example
 *   {"name": "not-ok.md", "setting": "💩", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect heading style type `💩`: use either `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {'atx'|'atx-closed'|'setext'} Type
 * @typedef {'consistent'|Type} Options
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {headingStyle} from 'mdast-util-heading-style'
import {generated} from 'unist-util-generated'

const remarkLintHeadingStyle = lintRule(
  'remark-lint:heading-style',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 'consistent') => {
    if (
      option !== 'consistent' &&
      option !== 'atx' &&
      option !== 'atx-closed' &&
      option !== 'setext'
    ) {
      file.fail(
        'Incorrect heading style type `' +
          option +
          "`: use either `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`"
      )
    }

    visit(tree, 'heading', (node) => {
      if (!generated(node)) {
        if (option === 'consistent') {
          // Funky nodes perhaps cannot be detected.
          /* c8 ignore next */
          option = headingStyle(node) || 'consistent'
        } else if (headingStyle(node, option) !== option) {
          file.message('Headings should use ' + option, node)
        }
      }
    })
  }
)

export default remarkLintHeadingStyle
