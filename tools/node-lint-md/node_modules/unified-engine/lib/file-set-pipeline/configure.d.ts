/**
 * @param {Context} context
 * @param {Settings} settings
 */
export function configure(context: Context, settings: Settings): void
export type Settings = import('./index.js').Settings
export type Context = {
  configuration?: Configuration | undefined
}
import {Configuration} from '../configuration.js'
