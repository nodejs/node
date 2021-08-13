/**
 * @type {Handle}
 * @param {Emphasis} node
 */
export function emphasis(
  node: Emphasis,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export namespace emphasis {
  export {emphasisPeek as peek}
}
export type Emphasis = import('mdast').Emphasis
export type Handle = import('../types.js').Handle
/**
 * @type {Handle}
 * @param {Emphasis} _
 */
declare function emphasisPeek(
  _: Emphasis,
  _1: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export {}
