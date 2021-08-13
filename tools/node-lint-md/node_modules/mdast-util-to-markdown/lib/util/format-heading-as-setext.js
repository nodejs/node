/**
 * @typedef {import('mdast').Heading} Heading
 * @typedef {import('../types.js').Context} Context
 */

import {visit, EXIT} from 'unist-util-visit'
import {toString} from 'mdast-util-to-string'

/**
 * @param {Heading} node
 * @param {Context} context
 * @returns {boolean}
 */
export function formatHeadingAsSetext(node, context) {
  let literalWithBreak = false

  // Look for literals with a line break.
  // Note that this also
  visit(node, (node) => {
    if (
      ('value' in node && /\r?\n|\r/.test(node.value)) ||
      node.type === 'break'
    ) {
      literalWithBreak = true
      return EXIT
    }
  })

  return Boolean(
    (!node.depth || node.depth < 3) &&
      toString(node) &&
      (context.options.setext || literalWithBreak)
  )
}
