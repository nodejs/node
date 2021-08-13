/**
 * @type {Handle}
 * @param {ListItem} node
 */
export function listItem(
  node: ListItem,
  parent: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export type ListItem = import('mdast').ListItem
export type List = import('mdast').List
export type Map = import('../util/indent-lines.js').Map
export type Options = import('../types.js').Options
export type Handle = import('../types.js').Handle
