/**
 * @typedef {import('mdast').Blockquote} Blockquote
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('../util/indent-lines.js').Map} Map
 */

import {containerFlow} from '../util/container-flow.js'
import {indentLines} from '../util/indent-lines.js'

/**
 * @type {Handle}
 * @param {Blockquote} node
 */
export function blockquote(node, _, context) {
  const exit = context.enter('blockquote')
  const value = indentLines(containerFlow(node, context), map)
  exit()
  return value
}

/** @type {Map} */
function map(line, _, blank) {
  return '>' + (blank ? '' : ' ') + line
}
