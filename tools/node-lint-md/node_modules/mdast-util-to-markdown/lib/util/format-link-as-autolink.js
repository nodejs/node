/**
 * @typedef {import('mdast').Link} Link
 * @typedef {import('../types.js').Context} Context
 */

import {toString} from 'mdast-util-to-string'

/**
 * @param {Link} node
 * @param {Context} context
 * @returns {boolean}
 */
export function formatLinkAsAutolink(node, context) {
  const raw = toString(node)

  return Boolean(
    !context.options.resourceLink &&
      // If there’s a url…
      node.url &&
      // And there’s a no title…
      !node.title &&
      // And the content of `node` is a single text node…
      node.children &&
      node.children.length === 1 &&
      node.children[0].type === 'text' &&
      // And if the url is the same as the content…
      (raw === node.url || 'mailto:' + raw === node.url) &&
      // And that starts w/ a protocol…
      /^[a-z][a-z+.-]+:/i.test(node.url) &&
      // And that doesn’t contain ASCII control codes (character escapes and
      // references don’t work) or angle brackets…
      !/[\0- <>\u007F]/.test(node.url)
  )
}
