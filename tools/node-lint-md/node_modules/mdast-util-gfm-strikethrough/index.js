/**
 * @typedef {import('mdast').Delete} Delete
 * @typedef {import('mdast-util-from-markdown').Extension} FromMarkdownExtension
 * @typedef {import('mdast-util-from-markdown').Handle} FromMarkdownHandle
 * @typedef {import('mdast-util-to-markdown').Options} ToMarkdownExtension
 * @typedef {import('mdast-util-to-markdown').Handle} ToMarkdownHandle
 */

import {containerPhrasing} from 'mdast-util-to-markdown/lib/util/container-phrasing.js'

/** @type {FromMarkdownExtension} */
export const gfmStrikethroughFromMarkdown = {
  canContainEols: ['delete'],
  enter: {strikethrough: enterStrikethrough},
  exit: {strikethrough: exitStrikethrough}
}

/** @type {ToMarkdownExtension} */
export const gfmStrikethroughToMarkdown = {
  unsafe: [{character: '~', inConstruct: 'phrasing'}],
  handlers: {delete: handleDelete}
}

handleDelete.peek = peekDelete

/** @type {FromMarkdownHandle} */
function enterStrikethrough(token) {
  this.enter({type: 'delete', children: []}, token)
}

/** @type {FromMarkdownHandle} */
function exitStrikethrough(token) {
  this.exit(token)
}

/**
 * @type {ToMarkdownHandle}
 * @param {Delete} node
 */
function handleDelete(node, _, context) {
  const exit = context.enter('emphasis')
  const value = containerPhrasing(node, context, {before: '~', after: '~'})
  exit()
  return '~~' + value + '~~'
}

/** @type {ToMarkdownHandle} */
function peekDelete() {
  return '~'
}
