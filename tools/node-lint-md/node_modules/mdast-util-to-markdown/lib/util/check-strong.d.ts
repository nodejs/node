/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */
/**
 * @param {Context} context
 * @returns {Exclude<Options['strong'], undefined>}
 */
export function checkStrong(
  context: Context
): Exclude<Options['strong'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
