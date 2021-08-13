/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-tabs
 * @fileoverview
 *   Warn when hard tabs (`\t`) are used instead of spaces.
 *
 *   ## Fix
 *
 *   [`remark-stringify`](https://github.com/remarkjs/remark/tree/HEAD/packages/remark-stringify)
 *   uses spaces where tabs are used for indentation, but retains tabs used in
 *   content.
 *
 *   See [Using remark to fix your Markdown](https://github.com/remarkjs/remark-lint#using-remark-to-fix-your-markdown)
 *   on how to automatically fix warnings for this rule.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   Foo Bar
 *
 *   ····Foo
 *
 * @example
 *   {"name": "not-ok.md", "label": "input", "positionless": true}
 *
 *   »Here's one before a code block.
 *
 *   Here's a tab:», and here is another:».
 *
 *   And this is in `inline»code`.
 *
 *   >»This is in a block quote.
 *
 *   *»And…
 *
 *   »1.»in a list.
 *
 *   And this is a tab as the last character.»
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1: Use spaces instead of tabs
 *   3:14: Use spaces instead of tabs
 *   3:37: Use spaces instead of tabs
 *   5:23: Use spaces instead of tabs
 *   7:2: Use spaces instead of tabs
 *   9:2: Use spaces instead of tabs
 *   11:1: Use spaces instead of tabs
 *   11:4: Use spaces instead of tabs
 *   13:41: Use spaces instead of tabs
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {location} from 'vfile-location'

const remarkLintNoTabs = lintRule(
  'remark-lint:no-tabs',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (_, file) => {
    const value = String(file)
    const toPoint = location(file).toPoint
    let index = value.indexOf('\t')

    while (index !== -1) {
      file.message('Use spaces instead of tabs', toPoint(index))
      index = value.indexOf('\t', index + 1)
    }
  }
)

export default remarkLintNoTabs
