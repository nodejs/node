/**
 * @typedef {import('mdast').ListItem} ListItem
 * @typedef {import('mdast').Paragraph} Paragraph
 * @typedef {import('mdast').BlockContent} BlockContent
 * @typedef {import('mdast-util-from-markdown').Extension} FromMarkdownExtension
 * @typedef {import('mdast-util-from-markdown').Handle} FromMarkdownHandle
 * @typedef {import('mdast-util-to-markdown').Options} ToMarkdownExtension
 * @typedef {import('mdast-util-to-markdown').Handle} ToMarkdownHandle
 */

import {listItem} from 'mdast-util-to-markdown/lib/handle/list-item.js'

/** @type {FromMarkdownExtension} */
export const gfmTaskListItemFromMarkdown = {
  exit: {
    taskListCheckValueChecked: exitCheck,
    taskListCheckValueUnchecked: exitCheck,
    paragraph: exitParagraphWithTaskListItem
  }
}

/** @type {ToMarkdownExtension} */
export const gfmTaskListItemToMarkdown = {
  unsafe: [{atBreak: true, character: '-', after: '[:|-]'}],
  handlers: {listItem: listItemWithTaskListItem}
}

/** @type {FromMarkdownHandle} */
function exitCheck(token) {
  // We’re always in a paragraph, in a list item.
  this.stack[this.stack.length - 2].checked =
    token.type === 'taskListCheckValueChecked'
}

/** @type {FromMarkdownHandle} */
function exitParagraphWithTaskListItem(token) {
  const parent = this.stack[this.stack.length - 2]
  /** @type {Paragraph} */
  // @ts-expect-error: must be true.
  const node = this.stack[this.stack.length - 1]
  /** @type {BlockContent[]} */
  // @ts-expect-error: check whether `parent` is a `listItem` later.
  const siblings = parent.children
  const head = node.children[0]
  let index = -1
  /** @type {Paragraph|undefined} */
  let firstParaghraph

  if (
    parent &&
    parent.type === 'listItem' &&
    typeof parent.checked === 'boolean' &&
    head &&
    head.type === 'text'
  ) {
    while (++index < siblings.length) {
      const sibling = siblings[index]
      if (sibling.type === 'paragraph') {
        firstParaghraph = sibling
        break
      }
    }

    if (firstParaghraph === node) {
      // Must start with a space or a tab.
      head.value = head.value.slice(1)

      if (head.value.length === 0) {
        node.children.shift()
      } else {
        // @ts-expect-error: must be true.
        head.position.start.column++
        // @ts-expect-error: must be true.
        head.position.start.offset++
        // @ts-expect-error: must be true.
        node.position.start = Object.assign({}, head.position.start)
      }
    }
  }

  this.exit(token)
}

/**
 * @type {ToMarkdownHandle}
 * @param {ListItem} node
 */
function listItemWithTaskListItem(node, parent, context) {
  const head = node.children[0]
  let value = listItem(node, parent, context)

  if (typeof node.checked === 'boolean' && head && head.type === 'paragraph') {
    value = value.replace(/^(?:[*+-]|\d+\.)([\r\n]| {1,3})/, check)
  }

  return value

  /**
   * @param {string} $0
   * @returns {string}
   */
  function check($0) {
    return $0 + '[' + (node.checked ? 'x' : ' ') + '] '
  }
}
