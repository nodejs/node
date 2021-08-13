/**
 * @typedef {import('./types.js').Handle} Handle
 * @typedef {import('./types.js').Comment} Comment
 */

import {stringifyEntities} from 'stringify-entities'

/**
 * @type {Handle}
 * @param {Comment} node
 */
export function comment(ctx, node) {
  // See: <https://html.spec.whatwg.org/multipage/syntax.html#comments>
  return ctx.bogusComments
    ? '<?' +
        stringifyEntities(
          node.value,
          Object.assign({}, ctx.entities, {subset: ['>']})
        ) +
        '>'
    : '<!--' + node.value.replace(/^>|^->|<!--|-->|--!>|<!-$/g, encode) + '-->'

  /**
   * @param {string} $0
   */
  function encode($0) {
    return stringifyEntities(
      $0,
      Object.assign({}, ctx.entities, {subset: ['<', '>']})
    )
  }
}
