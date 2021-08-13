/**
 * @type {Handle}
 * @param {LinkReference} node
 */
export function linkReference(
  node: LinkReference,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export namespace linkReference {
  export {linkReferencePeek as peek}
}
export type LinkReference = import('mdast').LinkReference
export type Handle = import('../types.js').Handle
/**
 * @type {Handle}
 */
declare function linkReferencePeek(): string
export {}
