/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */
/**
 * @param {Context} context
 * @returns {Exclude<Options['listItemIndent'], undefined>}
 */
export function checkListItemIndent(
  context: Context
): Exclude<Options['listItemIndent'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
