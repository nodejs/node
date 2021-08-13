/**
 * @type {Handle}
 * @param {InlineCode} node
 */
export function inlineCode(
  node: InlineCode,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export namespace inlineCode {
  export {inlineCodePeek as peek}
}
export type InlineCode = import('mdast').InlineCode
export type Handle = import('../types.js').Handle
/**
 * @type {Handle}
 */
declare function inlineCodePeek(): string
export {}
