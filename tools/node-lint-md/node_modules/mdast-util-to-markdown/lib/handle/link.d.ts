/**
 * @type {Handle}
 * @param {Link} node
 */
export function link(
  node: Link,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export namespace link {
  export {linkPeek as peek}
}
export type Link = import('mdast').Link
export type Handle = import('../types.js').Handle
export type Exit = import('../types.js').Exit
/**
 * @type {Handle}
 * @param {Link} node
 */
declare function linkPeek(
  node: Link,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export {}
