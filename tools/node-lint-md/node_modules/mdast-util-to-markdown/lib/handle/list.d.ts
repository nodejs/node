/**
 * @type {Handle}
 * @param {List} node
 */
export function list(
  node: List,
  parent: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export type List = import('mdast').List
export type Handle = import('../types.js').Handle
