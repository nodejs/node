/**
 * @typedef {import('mdast').Emphasis} Emphasis
 * @typedef {import('../types.js').Handle} Handle
 */

import {checkEmphasis} from '../util/check-emphasis.js'
import {containerPhrasing} from '../util/container-phrasing.js'

emphasis.peek = emphasisPeek

// To do: there are cases where emphasis cannot “form” depending on the
// previous or next character of sequences.
// There’s no way around that though, except for injecting zero-width stuff.
// Do we need to safeguard against that?
/**
 * @type {Handle}
 * @param {Emphasis} node
 */
export function emphasis(node, _, context) {
  const marker = checkEmphasis(context)
  const exit = context.enter('emphasis')
  const value = containerPhrasing(node, context, {
    before: marker,
    after: marker
  })
  exit()
  return marker + value + marker
}

/**
 * @type {Handle}
 * @param {Emphasis} _
 */
function emphasisPeek(_, _1, context) {
  return context.options.emphasis || '*'
}
