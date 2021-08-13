/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module fenced-code-flag
 * @fileoverview
 *   Check fenced code block flags.
 *
 *   Options: `Array.<string>` or `Object`, optional.
 *
 *   Providing an array is as passing `{flags: Array}`.
 *
 *   The object can have an array of `'flags'` which are allowed: other flags
 *   will not be allowed.
 *   An `allowEmpty` field (`boolean`, default: `false`) can be set to allow
 *   code blocks without language flags.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ```
 *   alpha()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-3:4: Missing code language flag
 *
 * @example
 *   {"name": "ok.md", "setting": {"allowEmpty": true}}
 *
 *   ```
 *   alpha()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "setting": {"allowEmpty": false}, "label": "input"}
 *
 *   ```
 *   alpha()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "setting": {"allowEmpty": false}, "label": "output"}
 *
 *   1:1-3:4: Missing code language flag
 *
 * @example
 *   {"name": "ok.md", "setting": ["alpha"]}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "ok.md", "setting": {"flags":["alpha"]}}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "setting": ["charlie"], "label": "input"}
 *
 *   ```alpha
 *   bravo()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "setting": ["charlie"], "label": "output"}
 *
 *   1:1-3:4: Incorrect code language flag
 */

/**
 * @typedef {import('mdast').Root} Root
 *
 * @typedef {string[]} Flags
 *
 * @typedef FlagMap
 * @property {Flags} [flags]
 * @property {boolean} [allowEmpty=false]
 *
 * @typedef {Flags|FlagMap} Options
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart, pointEnd} from 'unist-util-position'
import {generated} from 'unist-util-generated'

const fence = /^ {0,3}([~`])\1{2,}/

const remarkLintFencedCodeFlag = lintRule(
  'remark-lint:fenced-code-flag',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option) => {
    const value = String(file)
    let allowEmpty = false
    /** @type {string[]} */
    let allowed = []

    if (typeof option === 'object') {
      if (Array.isArray(option)) {
        allowed = option
      } else {
        allowEmpty = Boolean(option.allowEmpty)

        if (option.flags) {
          allowed = option.flags
        }
      }
    }

    visit(tree, 'code', (node) => {
      if (!generated(node)) {
        if (node.lang) {
          if (allowed.length > 0 && !allowed.includes(node.lang)) {
            file.message('Incorrect code language flag', node)
          }
        } else {
          const slice = value.slice(
            pointStart(node).offset,
            pointEnd(node).offset
          )

          if (!allowEmpty && fence.test(slice)) {
            file.message('Missing code language flag', node)
          }
        }
      }
    })
  }
)

export default remarkLintFencedCodeFlag
