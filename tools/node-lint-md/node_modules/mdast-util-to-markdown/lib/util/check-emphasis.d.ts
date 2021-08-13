/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */
/**
 * @param {Context} context
 * @returns {Exclude<Options['emphasis'], undefined>}
 */
export function checkEmphasis(
  context: Context
): Exclude<Options['emphasis'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
