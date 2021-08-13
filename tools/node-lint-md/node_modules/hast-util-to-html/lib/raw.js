/**
 * @typedef {import('./types.js').Handle} Handle
 * @typedef {import('./types.js').Raw} Raw
 */

import {text} from './text.js'

/**
 * @type {Handle}
 * @param {Raw} node
 */
export function raw(ctx, node, index, parent) {
  // @ts-ignore Hush.
  return ctx.dangerous ? node.value : text(ctx, node, index, parent)
}
