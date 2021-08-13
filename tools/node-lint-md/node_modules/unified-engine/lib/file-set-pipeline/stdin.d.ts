/**
 * @param {Context} context
 * @param {Settings} settings
 * @param {Callback} next
 */
export function stdin(
  context: Context,
  settings: Settings,
  next: Callback
): void
export type VFile = import('vfile').VFile
export type Callback = import('trough').Callback
export type Settings = import('./index.js').Settings
export type Context = {
  files: Array<string | VFile>
}
