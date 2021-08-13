/**
 * Write a virtual file to `streamOut`.
 * Ignored when `output` is given, more than one file was processed, or `out`
 * is false.
 *
 * @param {Context} context
 * @param {VFile} file
 * @param {Callback} next
 */
export function stdout(context: Context, file: VFile, next: Callback): void
export type VFile = import('vfile').VFile
export type Callback = import('trough').Callback
export type Context = import('./index.js').Context
