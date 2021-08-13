/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module file-extension
 * @fileoverview
 *   Warn when the file extension differ from the preferred extension.
 *
 *   Does not warn when given documents have no file extensions (such as
 *   `AUTHORS` or `LICENSE`).
 *
 *   Options: `string`, default: `'md'` — Expected file extension.
 *
 * @example
 *   {"name": "readme.md"}
 *
 * @example
 *   {"name": "readme"}
 *
 * @example
 *   {"name": "readme.mkd", "label": "output", "positionless": true}
 *
 *   1:1: Incorrect extension: use `md`
 *
 * @example
 *   {"name": "readme.mkd", "setting": "mkd"}
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {string} Options
 */

import {lintRule} from 'unified-lint-rule'

const remarkLintFileExtension = lintRule(
  'remark-lint:file-extension',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (_, file, option = 'md') => {
    const ext = file.extname

    if (ext && ext.slice(1) !== option) {
      file.message('Incorrect extension: use `' + option + '`')
    }
  }
)

export default remarkLintFileExtension
