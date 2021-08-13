/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shortcut-reference-link
 * @fileoverview
 *   Warn when shortcut reference links are used.
 *
 *   Shortcut references render as links when a definition is found, and as
 *   plain text without definition.
 *   Sometimes, you don’t intend to create a link from the reference, but this
 *   rule still warns anyway.
 *   In that case, you can escape the reference like so: `\[foo]`.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   [foo][]
 *
 *   [foo]: http://foo.bar/baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   [foo]
 *
 *   [foo]: http://foo.bar/baz
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:6: Use the trailing `[]` on reference links
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {generated} from 'unist-util-generated'

const remarkLintNoShortcutReferenceLink = lintRule(
  'remark-lint:no-shortcut-reference-link',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit(tree, 'linkReference', (node) => {
      if (!generated(node) && node.referenceType === 'shortcut') {
        file.message('Use the trailing `[]` on reference links', node)
      }
    })
  }
)

export default remarkLintNoShortcutReferenceLink
