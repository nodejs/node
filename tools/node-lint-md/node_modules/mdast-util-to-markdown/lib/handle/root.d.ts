/**
 * @type {Handle}
 * @param {Root} node
 */
export function root(
  node: Root,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export type Root = import('mdast').Root
export type Handle = import('../types.js').Handle
