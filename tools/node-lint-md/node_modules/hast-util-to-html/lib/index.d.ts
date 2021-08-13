/**
 * @param {Node|Array.<Node>} node
 * @param {Options} [options]
 * @returns {string}
 */
export function toHtml(node: Node | Array<Node>, options?: Options): string
export type Node = import('./types.js').Node
export type Options = import('./types.js').Options
export type Context = import('./types.js').Context
export type Quote = import('./types.js').Quote
