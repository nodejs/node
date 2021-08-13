/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */
/**
 * @param {Context} context
 * @returns {Exclude<Options['rule'], undefined>}
 */
export function checkRule(context: Context): Exclude<Options['rule'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
