/**
 * @param {Link} node
 * @param {Context} context
 * @returns {boolean}
 */
export function formatLinkAsAutolink(node: Link, context: Context): boolean
export type Link = import('mdast').Link
export type Context = import('../types.js').Context
