/**
 * @typedef {import('mdast').Code} Code
 * @typedef {import('../types.js').Context} Context
 */
/**
 * @param {Code} node
 * @param {Context} context
 * @returns {boolean}
 */
export function formatCodeAsIndented(node: Code, context: Context): boolean
export type Code = import('mdast').Code
export type Context = import('../types.js').Context
