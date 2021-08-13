/**
 * @typedef {import('mdast').Strong} Strong
 * @typedef {import('../types.js').Handle} Handle
 */

import {checkStrong} from '../util/check-strong.js'
import {containerPhrasing} from '../util/container-phrasing.js'

strong.peek = strongPeek

// To do: there are cases where emphasis cannot “form” depending on the
// previous or next character of sequences.
// There’s no way around that though, except for injecting zero-width stuff.
// Do we need to safeguard against that?
/**
 * @type {Handle}
 * @param {Strong} node
 */
export function strong(node, _, context) {
  const marker = checkStrong(context)
  const exit = context.enter('strong')
  const value = containerPhrasing(node, context, {
    before: marker,
    after: marker
  })
  exit()
  return marker + marker + value + marker + marker
}

/**
 * @type {Handle}
 * @param {Strong} _
 */
function strongPeek(_, _1, context) {
  return context.options.strong || '*'
}
