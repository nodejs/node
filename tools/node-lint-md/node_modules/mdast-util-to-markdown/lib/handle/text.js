/**
 * @typedef {import('mdast').Text} Text
 * @typedef {import('../types.js').Handle} Handle
 */

import {safe} from '../util/safe.js'

/**
 * @type {Handle}
 * @param {Text} node
 */
export function text(node, _, context, safeOptions) {
  return safe(context, node.value, safeOptions)
}
