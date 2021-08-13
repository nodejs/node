/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-outer-dashes
 * @fileoverview
 *   Warn when file names contain initial or final dashes (hyphen-minus, `-`).
 *
 * @example
 *   {"name": "readme.md"}
 *
 * @example
 *   {"name": "-readme.md", "label": "output", "positionless": true}
 *
 *   1:1: Do not use initial or final dashes in a file name
 *
 * @example
 *   {"name": "readme-.md", "label": "output", "positionless": true}
 *
 *   1:1: Do not use initial or final dashes in a file name
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'

const remarkLintNofileNameOuterDashes = lintRule(
  'remark-lint:no-file-name-outer-dashes',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (_, file) => {
    if (file.stem && /^-|-$/.test(file.stem)) {
      file.message('Do not use initial or final dashes in a file name')
    }
  }
)

export default remarkLintNofileNameOuterDashes
