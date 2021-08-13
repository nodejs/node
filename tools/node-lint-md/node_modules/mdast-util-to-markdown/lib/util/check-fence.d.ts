/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */
/**
 * @param {Context} context
 * @returns {Exclude<Options['fence'], undefined>}
 */
export function checkFence(
  context: Context
): Exclude<Options['fence'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
