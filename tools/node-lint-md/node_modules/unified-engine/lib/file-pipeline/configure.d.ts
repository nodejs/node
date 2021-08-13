/**
 * Collect configuration for a file based on the context.
 *
 * @param {Context} context
 * @param {VFile} file
 * @param {Callback} next
 */
export function configure(context: Context, file: VFile, next: Callback): void
export type VFile = import('vfile').VFile
export type Callback = import('trough').Callback
export type Context = import('./index.js').Context
