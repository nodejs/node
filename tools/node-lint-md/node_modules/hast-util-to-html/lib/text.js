/**
 * @typedef {import('./types.js').Handle} Handle
 * @typedef {import('./types.js').Text} Text
 */

import {stringifyEntities} from 'stringify-entities'

/**
 * @type {Handle}
 * @param {Text} node
 */
export function text(ctx, node, _, parent) {
  // Check if content of `node` should be escaped.
  return parent &&
    parent.type === 'element' &&
    // @ts-expect-error: hush.
    (parent.tagName === 'script' || parent.tagName === 'style')
    ? node.value
    : stringifyEntities(
        node.value,
        Object.assign({}, ctx.entities, {subset: ['<', '&']})
      )
}
