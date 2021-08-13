/**
 * @type {Handle}
 * @param {Strong} node
 */
export function strong(
  node: Strong,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export namespace strong {
  export {strongPeek as peek}
}
export type Strong = import('mdast').Strong
export type Handle = import('../types.js').Handle
/**
 * @type {Handle}
 * @param {Strong} _
 */
declare function strongPeek(
  _: Strong,
  _1: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export {}
