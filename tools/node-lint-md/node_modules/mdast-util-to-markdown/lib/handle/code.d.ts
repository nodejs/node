/**
 * @type {Handle}
 * @param {Code} node
 */
export function code(
  node: Code,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export type Code = import('mdast').Code
export type Handle = import('../types.js').Handle
export type Exit = import('../types.js').Exit
export type Map = import('../util/indent-lines.js').Map
