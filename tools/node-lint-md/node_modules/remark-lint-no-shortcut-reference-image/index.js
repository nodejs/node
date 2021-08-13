/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shortcut-reference-image
 * @fileoverview
 *   Warn when shortcut reference images are used.
 *
 *   Shortcut references render as images when a definition is found, and as
 *   plain text without definition.
 *   Sometimes, you don’t intend to create an image from the reference, but this
 *   rule still warns anyway.
 *   In that case, you can escape the reference like so: `!\[foo]`.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   ![foo][]
 *
 *   [foo]: http://foo.bar/baz.png
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ![foo]
 *
 *   [foo]: http://foo.bar/baz.png
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:7: Use the trailing [] on reference images
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {generated} from 'unist-util-generated'

const remarkLintNoShortcutReferenceImage = lintRule(
  'remark-lint:no-shortcut-reference-image',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit(tree, 'imageReference', (node) => {
      if (!generated(node) && node.referenceType === 'shortcut') {
        file.message('Use the trailing [] on reference images', node)
      }
    })
  }
)

export default remarkLintNoShortcutReferenceImage
