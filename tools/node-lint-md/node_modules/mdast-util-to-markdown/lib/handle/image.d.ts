/**
 * @type {Handle}
 * @param {Image} node
 */
export function image(
  node: Image,
  _: import('../types.js').Parent | null | undefined,
  context: import('../types.js').Context
): string
export namespace image {
  export {imagePeek as peek}
}
export type Image = import('mdast').Image
export type Handle = import('../types.js').Handle
/**
 * @type {Handle}
 */
declare function imagePeek(): string
export {}
