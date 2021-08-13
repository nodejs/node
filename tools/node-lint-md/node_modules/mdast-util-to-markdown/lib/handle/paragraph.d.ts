/**
 * @type {Handle}
 * @param {Paragraph} node
 */
export function paragraph(
  node: Paragraph,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export type Paragraph = import('mdast').Paragraph
export type Handle = import('../types.js').Handle
