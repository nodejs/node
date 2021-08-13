/**
 * @type {Handle}
 * @param {Blockquote} node
 */
export function blockquote(
  node: Blockquote,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export type Blockquote = import('mdast').Blockquote
export type Handle = import('../types.js').Handle
export type Map = import('../util/indent-lines.js').Map
