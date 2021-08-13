/**
 * @type {Handle}
 * @param {HTML} node
 */
export function html(node: HTML): string
export namespace html {
  export {htmlPeek as peek}
}
export type HTML = import('mdast').HTML
export type Handle = import('../types.js').Handle
/**
 * @type {Handle}
 */
declare function htmlPeek(): string
export {}
