/**
 * @typedef {import('../types.js').OmitHandle} OmitHandle
 * @typedef {import('../types.js').Child} Child
 */

import {isElement} from 'hast-util-is-element'
import {comment} from './util/comment.js'
import {siblingBefore, siblingAfter} from './util/siblings.js'
import {whitespaceStart} from './util/whitespace-start.js'
import {closing} from './closing.js'
import {omission} from './omission.js'

export const opening = omission({
  html,
  head,
  body,
  colgroup,
  tbody
})

/**
 * Whether to omit `<html>`.
 *
 * @type {OmitHandle}
 */
function html(node) {
  const head = siblingAfter(node, -1)
  return !head || !comment(head)
}

/**
 * Whether to omit `<head>`.
 *
 * @type {OmitHandle}
 */
function head(node) {
  const children = node.children
  /** @type {Array.<string>} */
  const seen = []
  let index = -1
  /** @type {Child} */
  let child

  while (++index < children.length) {
    child = children[index]
    if (isElement(child, ['title', 'base'])) {
      if (seen.includes(child.tagName)) return false
      seen.push(child.tagName)
    }
  }

  return children.length > 0
}

/**
 * Whether to omit `<body>`.
 *
 * @type {OmitHandle}
 */
function body(node) {
  const head = siblingAfter(node, -1, true)

  return (
    !head ||
    (!comment(head) &&
      !whitespaceStart(head) &&
      !isElement(head, ['meta', 'link', 'script', 'style', 'template']))
  )
}

/**
 * Whether to omit `<colgroup>`.
 * The spec describes some logic for the opening tag, but it’s easier to
 * implement in the closing tag, to the same effect, so we handle it there
 * instead.
 *
 * @type {OmitHandle}
 */
function colgroup(node, index, parent) {
  const previous = siblingBefore(parent, index)
  const head = siblingAfter(node, -1, true)

  // Previous colgroup was already omitted.
  if (
    isElement(previous, 'colgroup') &&
    closing(previous, parent.children.indexOf(previous), parent)
  ) {
    return false
  }

  return head && isElement(head, 'col')
}

/**
 * Whether to omit `<tbody>`.
 *
 * @type {OmitHandle}
 */
function tbody(node, index, parent) {
  const previous = siblingBefore(parent, index)
  const head = siblingAfter(node, -1)

  // Previous table section was already omitted.
  if (
    isElement(previous, ['thead', 'tbody']) &&
    closing(previous, parent.children.indexOf(previous), parent)
  ) {
    return false
  }

  return head && isElement(head, 'tr')
}
