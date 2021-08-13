/**
 * @type {Handle}
 * @param {Definition} node
 */
export function definition(
  node: Definition,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export type Definition = import('mdast').Definition
export type Handle = import('../types.js').Handle
