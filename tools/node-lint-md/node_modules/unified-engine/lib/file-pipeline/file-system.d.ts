/**
 * Write a virtual file to the file-system.
 * Ignored when `output` is not given.
 *
 * @param {Context} context
 * @param {VFile} file
 * @param {Callback} next
 */
export function fileSystem(context: Context, file: VFile, next: Callback): void
export type VFile = import('vfile').VFile
export type Callback = import('trough').Callback
export type Context = import('./index.js').Context
