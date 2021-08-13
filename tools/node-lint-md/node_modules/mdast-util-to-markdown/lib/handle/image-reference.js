/**
 * @typedef {import('mdast').ImageReference} ImageReference
 * @typedef {import('../types.js').Handle} Handle
 */

import {association} from '../util/association.js'
import {safe} from '../util/safe.js'

imageReference.peek = imageReferencePeek

/**
 * @type {Handle}
 * @param {ImageReference} node
 */
export function imageReference(node, _, context) {
  const type = node.referenceType
  const exit = context.enter('imageReference')
  let subexit = context.enter('label')
  const alt = safe(context, node.alt, {before: '[', after: ']'})
  let value = '![' + alt + ']'

  subexit()
  // Hide the fact that we’re in phrasing, because escapes don’t work.
  const stack = context.stack
  context.stack = []
  subexit = context.enter('reference')
  const reference = safe(context, association(node), {before: '[', after: ']'})
  subexit()
  context.stack = stack
  exit()

  if (type === 'full' || !alt || alt !== reference) {
    value += '[' + reference + ']'
  } else if (type !== 'shortcut') {
    value += '[]'
  }

  return value
}

/**
 * @type {Handle}
 */
function imageReferencePeek() {
  return '!'
}
