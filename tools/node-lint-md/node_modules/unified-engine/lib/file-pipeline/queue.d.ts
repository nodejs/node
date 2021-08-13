/**
 * Queue all files which came this far.
 * When the last file gets here, run the file-set pipeline and flush the queue.
 *
 * @param {Context} context
 * @param {VFile} file
 * @param {Callback} next
 */
export function queue(context: Context, file: VFile, next: Callback): void
export type VFile = import('vfile').VFile
export type Callback = import('trough').Callback
export type Context = import('./index.js').Context
