/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */
/**
 * @param {Context} context
 * @returns {Exclude<Options['bullet'], undefined>}
 */
export function checkBullet(
  context: Context
): Exclude<Options['bullet'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
