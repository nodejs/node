/**
 * @type {Handle}
 */
export function one(
  ctx: import('./types.js').Context,
  node: import('./types.js').Node,
  index: number,
  parent: import('hast').Parent
): string
/**
 * Serialize all children of `parent`.
 *
 * @type {Handle}
 * @param {Parent} parent
 */
export function all(ctx: import('./types.js').Context, parent: Parent): string
/**
 * @type {Handle}
 * @param {Element} node
 */
export function element(
  ctx: import('./types.js').Context,
  node: Element,
  index: number,
  parent: import('hast').Parent
): string
export type Handle = import('./types.js').Handle
export type Element = import('./types.js').Element
export type Context = import('./types.js').Context
export type Properties = import('./types.js').Properties
export type PropertyValue = import('./types.js').PropertyValue
export type Parent = import('./types.js').Parent
