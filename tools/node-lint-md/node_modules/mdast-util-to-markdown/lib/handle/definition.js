/**
 * @typedef {import('mdast').Definition} Definition
 * @typedef {import('../types.js').Handle} Handle
 */

import {association} from '../util/association.js'
import {checkQuote} from '../util/check-quote.js'
import {safe} from '../util/safe.js'

/**
 * @type {Handle}
 * @param {Definition} node
 */
export function definition(node, _, context) {
  const marker = checkQuote(context)
  const suffix = marker === '"' ? 'Quote' : 'Apostrophe'
  const exit = context.enter('definition')
  let subexit = context.enter('label')
  let value =
    '[' + safe(context, association(node), {before: '[', after: ']'}) + ']: '

  subexit()

  if (
    // If there’s no url, or…
    !node.url ||
    // If there’s whitespace, enclosed is prettier.
    /[ \t\r\n]/.test(node.url)
  ) {
    subexit = context.enter('destinationLiteral')
    value += '<' + safe(context, node.url, {before: '<', after: '>'}) + '>'
  } else {
    // No whitespace, raw is prettier.
    subexit = context.enter('destinationRaw')
    value += safe(context, node.url, {before: ' ', after: ' '})
  }

  subexit()

  if (node.title) {
    subexit = context.enter('title' + suffix)
    value +=
      ' ' +
      marker +
      safe(context, node.title, {before: marker, after: marker}) +
      marker
    subexit()
  }

  exit()

  return value
}
