/**
 * @param {Context} context
 * @param {Settings} settings
 * @param {Callback} next
 */
export function fileSystem(
  context: Context,
  settings: Settings,
  next: Callback
): void
export type VFile = import('vfile').VFile
export type Callback = import('trough').Callback
export type Settings = import('./index.js').Settings
export type Configuration = import('./index.js').Configuration
export type Context = {
  files: Array<string | VFile>
  configuration?: import('../configuration.js').Configuration | undefined
}
