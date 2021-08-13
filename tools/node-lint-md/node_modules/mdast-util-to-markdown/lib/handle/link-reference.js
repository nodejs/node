/**
 * @typedef {import('mdast').LinkReference} LinkReference
 * @typedef {import('../types.js').Handle} Handle
 */

import {association} from '../util/association.js'
import {containerPhrasing} from '../util/container-phrasing.js'
import {safe} from '../util/safe.js'

linkReference.peek = linkReferencePeek

/**
 * @type {Handle}
 * @param {LinkReference} node
 */
export function linkReference(node, _, context) {
  const type = node.referenceType
  const exit = context.enter('linkReference')
  let subexit = context.enter('label')
  const text = containerPhrasing(node, context, {before: '[', after: ']'})
  let value = '[' + text + ']'

  subexit()
  // Hide the fact that we’re in phrasing, because escapes don’t work.
  const stack = context.stack
  context.stack = []
  subexit = context.enter('reference')
  const reference = safe(context, association(node), {before: '[', after: ']'})
  subexit()
  context.stack = stack
  exit()

  if (type === 'full' || !text || text !== reference) {
    value += '[' + reference + ']'
  } else if (type !== 'shortcut') {
    value += '[]'
  }

  return value
}

/**
 * @type {Handle}
 */
function linkReferencePeek() {
  return '['
}
