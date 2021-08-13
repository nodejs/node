/**
 * @typedef {import('mdast').Paragraph} Paragraph
 * @typedef {import('../types.js').Handle} Handle
 */

import {containerPhrasing} from '../util/container-phrasing.js'

/**
 * @type {Handle}
 * @param {Paragraph} node
 */
export function paragraph(node, _, context) {
  const exit = context.enter('paragraph')
  const subexit = context.enter('phrasing')
  const value = containerPhrasing(node, context, {before: '\n', after: '\n'})
  subexit()
  exit()
  return value
}
