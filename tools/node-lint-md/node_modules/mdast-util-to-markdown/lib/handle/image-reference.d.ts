/**
 * @type {Handle}
 * @param {ImageReference} node
 */
export function imageReference(
  node: ImageReference,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export namespace imageReference {
  export {imageReferencePeek as peek}
}
export type ImageReference = import('mdast').ImageReference
export type Handle = import('../types.js').Handle
/**
 * @type {Handle}
 */
declare function imageReferencePeek(): string
export {}
