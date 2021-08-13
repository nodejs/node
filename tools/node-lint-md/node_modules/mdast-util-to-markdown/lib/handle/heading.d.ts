/**
 * @type {Handle}
 * @param {Heading} node
 */
export function heading(
  node: Heading,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export type Heading = import('mdast').Heading
export type Handle = import('../types.js').Handle
export type Exit = import('../types.js').Exit
