/**
 * @param {Context} context
 * @returns {Exclude<Options['bulletOrdered'], undefined>}
 */
export function checkBulletOrderedOther(
  context: Context
): Exclude<Options['bulletOrdered'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
