/**
 * @type {Handle}
 * @param {Text} node
 */
export function text(
  node: Text,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context,
  safeOptions: import('../types.js').SafeOptions
): string
export type Text = import('mdast').Text
export type Handle = import('../types.js').Handle
