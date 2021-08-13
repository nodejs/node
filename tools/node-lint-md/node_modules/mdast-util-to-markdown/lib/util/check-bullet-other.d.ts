/**
 * @param {Context} context
 * @returns {Exclude<Options['bullet'], undefined>}
 */
export function checkBulletOther(
  context: Context
): Exclude<Options['bullet'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
