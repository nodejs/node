/**
 * @typedef {import('../types.js').Context} Context
 * @typedef {import('../types.js').Options} Options
 */
/**
 * @param {Context} context
 * @returns {Exclude<Options['ruleRepetition'], undefined>}
 */
export function checkRuleRepetition(
  context: Context
): Exclude<Options['ruleRepetition'], undefined>
export type Context = import('../types.js').Context
export type Options = import('../types.js').Options
