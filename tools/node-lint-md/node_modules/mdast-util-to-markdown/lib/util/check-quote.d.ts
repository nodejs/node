/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */
/**
 * @param {Context} context
 * @returns {Exclude<Options['quote'], undefined>}
 */
export function checkQuote(
  context: Context
): Exclude<Options['quote'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
