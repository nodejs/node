/**
 * @typedef {import('./index.js').Settings} Settings
 */

/**
 * @typedef Context
 * @property {Configuration} [configuration]
 */

import {Configuration} from '../configuration.js'

/**
 * @param {Context} context
 * @param {Settings} settings
 */
export function configure(context, settings) {
  context.configuration = new Configuration(settings)
}
