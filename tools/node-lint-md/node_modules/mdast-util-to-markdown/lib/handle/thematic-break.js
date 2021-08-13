/**
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('mdast').ThematicBreak} ThematicBreak
 */

import {checkRuleRepetition} from '../util/check-rule-repetition.js'
import {checkRule} from '../util/check-rule.js'

/**
 * @type {Handle}
 * @param {ThematicBreak} _
 */
export function thematicBreak(_, _1, context) {
  const value = (
    checkRule(context) + (context.options.ruleSpaces ? ' ' : '')
  ).repeat(checkRuleRepetition(context))

  return context.options.ruleSpaces ? value.slice(0, -1) : value
}
