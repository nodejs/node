/**
 * @param {Node} tree
 * @param {Options} [options]
 * @returns {string}
 */
export function toMarkdown(
  tree: Node,
  options?: import('./types.js').Options | undefined
): string
export type Node = import('./types.js').Node
export type Options = import('./types.js').Options
export type Context = import('./types.js').Context
export type Handle = import('./types.js').Handle
export type Join = import('./types.js').Join
export type Unsafe = import('./types.js').Unsafe
