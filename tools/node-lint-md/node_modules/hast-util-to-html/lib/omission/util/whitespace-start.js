/**
 * @typedef {import('../../types.js').Node} Node
 * @typedef {import('../../types.js').Text} Text
 */

import {convert} from 'unist-util-is'
import {whitespace} from 'hast-util-whitespace'

/** @type {import('unist-util-is').AssertPredicate<Text>} */
// @ts-ignore
const isText = convert('text')

/**
 * Check if `node` starts with whitespace.
 *
 * @param {Node} node
 * @returns {boolean}
 */
export function whitespaceStart(node) {
  return isText(node) && whitespace(node.value.charAt(0))
}
