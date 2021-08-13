/**
 * @type {Handle}
 * @param {Text} node
 */
export function text(
  ctx: import('./types.js').Context,
  node: Text,
  _: number,
  parent: import('hast').Parent
): string
export type Handle = import('./types.js').Handle
export type Text = import('./types.js').Text
