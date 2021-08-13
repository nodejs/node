import {core} from './core.js'
import {formatSmart} from './util/format-smart.js'
import {formatBasic} from './util/format-basic.js'

/**
 * @typedef {import('./core.js').CoreOptions & import('./util/format-smart.js').FormatSmartOptions} StringifyEntitiesOptions
 * @typedef {import('./core.js').CoreOptions} StringifyEntitiesLightOptions
 */

/**
 * Encode special characters in `value`.
 * @param {string} value
 * @param {StringifyEntitiesOptions} [options]
 */
export function stringifyEntities(value, options) {
  return core(value, Object.assign({format: formatSmart}, options))
}

/**
 * Encode special characters in `value` as hexadecimals.
 * @param {string} value
 * @param {StringifyEntitiesLightOptions} [options]
 */
export function stringifyEntitiesLight(value, options) {
  return core(value, Object.assign({format: formatBasic}, options))
}
