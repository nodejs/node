/**
 * @typedef {import('mdast').Link} Link
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('../types.js').Exit} Exit
 */

import {checkQuote} from '../util/check-quote.js'
import {formatLinkAsAutolink} from '../util/format-link-as-autolink.js'
import {containerPhrasing} from '../util/container-phrasing.js'
import {safe} from '../util/safe.js'

link.peek = linkPeek

/**
 * @type {Handle}
 * @param {Link} node
 */
export function link(node, _, context) {
  const quote = checkQuote(context)
  const suffix = quote === '"' ? 'Quote' : 'Apostrophe'
  /** @type {Exit} */
  let exit
  /** @type {Exit} */
  let subexit
  /** @type {string} */
  let value

  if (formatLinkAsAutolink(node, context)) {
    // Hide the fact that we’re in phrasing, because escapes don’t work.
    const stack = context.stack
    context.stack = []
    exit = context.enter('autolink')
    value =
      '<' + containerPhrasing(node, context, {before: '<', after: '>'}) + '>'
    exit()
    context.stack = stack
    return value
  }

  exit = context.enter('link')
  subexit = context.enter('label')
  value =
    '[' + containerPhrasing(node, context, {before: '[', after: ']'}) + ']('
  subexit()

  if (
    // If there’s no url but there is a title…
    (!node.url && node.title) ||
    // Or if there’s markdown whitespace or an eol, enclose.
    /[ \t\r\n]/.test(node.url)
  ) {
    subexit = context.enter('destinationLiteral')
    value += '<' + safe(context, node.url, {before: '<', after: '>'}) + '>'
  } else {
    // No whitespace, raw is prettier.
    subexit = context.enter('destinationRaw')
    value += safe(context, node.url, {
      before: '(',
      after: node.title ? ' ' : ')'
    })
  }

  subexit()

  if (node.title) {
    subexit = context.enter('title' + suffix)
    value +=
      ' ' +
      quote +
      safe(context, node.title, {before: quote, after: quote}) +
      quote
    subexit()
  }

  value += ')'

  exit()
  return value
}

/**
 * @type {Handle}
 * @param {Link} node
 */
function linkPeek(node, _, context) {
  return formatLinkAsAutolink(node, context) ? '<' : '['
}
