/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */
/**
 * @param {Context} context
 * @returns {Exclude<Options['bulletOrdered'], undefined>}
 */
export function checkBulletOrdered(
  context: Context
): Exclude<Options['bulletOrdered'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
