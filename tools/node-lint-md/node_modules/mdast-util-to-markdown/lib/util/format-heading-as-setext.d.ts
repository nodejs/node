/**
 * @param {Heading} node
 * @param {Context} context
 * @returns {boolean}
 */
export function formatHeadingAsSetext(node: Heading, context: Context): boolean
export type Heading = import('mdast').Heading
export type Context = import('../types.js').Context
