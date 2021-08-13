/**
 * @typedef {import('./types.js').Node} Node
 * @typedef {import('./types.js').Options} Options
 * @typedef {import('./types.js').Context} Context
 * @typedef {import('./types.js').Quote} Quote
 */

import {html, svg} from 'property-information'
import {htmlVoidElements} from 'html-void-elements'
import {omission} from './omission/index.js'
import {one} from './tree.js'

/**
 * @param {Node|Array.<Node>} node
 * @param {Options} [options]
 * @returns {string}
 */
export function toHtml(node, options = {}) {
  const quote = options.quote || '"'
  /** @type {Quote} */
  const alternative = quote === '"' ? "'" : '"'

  if (quote !== '"' && quote !== "'") {
    throw new Error('Invalid quote `' + quote + '`, expected `\'` or `"`')
  }

  /** @type {Context} */
  const context = {
    valid: options.allowParseErrors ? 0 : 1,
    safe: options.allowDangerousCharacters ? 0 : 1,
    schema: options.space === 'svg' ? svg : html,
    omit: options.omitOptionalTags ? omission : undefined,
    quote,
    alternative,
    smart: options.quoteSmart,
    unquoted: options.preferUnquoted,
    tight: options.tightAttributes,
    upperDoctype: options.upperDoctype,
    tightDoctype: options.tightDoctype,
    bogusComments: options.bogusComments,
    tightLists: options.tightCommaSeparatedLists,
    tightClose: options.tightSelfClosing,
    collapseEmpty: options.collapseEmptyAttributes,
    dangerous: options.allowDangerousHtml,
    voids: options.voids || htmlVoidElements.concat(),
    entities: options.entities || {},
    close: options.closeSelfClosing,
    closeEmpty: options.closeEmptyElements
  }

  return one(
    context,
    // @ts-ignore Assume `node` does not contain a root.
    Array.isArray(node) ? {type: 'root', children: node} : node,
    null,
    null
  )
}
