/**
 * @typedef {import('./types.js').Handle} Handle
 */

/**
 * @type {Handle}
 */
export function doctype(ctx) {
  return (
    '<!' +
    (ctx.upperDoctype ? 'DOCTYPE' : 'doctype') +
    (ctx.tightDoctype ? '' : ' ') +
    'html>'
  )
}
