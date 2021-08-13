/**
 * Transform the tree associated with a file with configured plugins.
 *
 * @param {Context} context
 * @param {VFile} file
 * @param {Callback} next
 */
export function transform(context: Context, file: VFile, next: Callback): void
export type VFile = import('vfile').VFile
export type Callback = import('trough').Callback
export type Context = import('./index.js').Context
