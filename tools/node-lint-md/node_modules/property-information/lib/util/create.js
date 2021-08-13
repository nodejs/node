import {normalize} from '../normalize.js'
import {Schema} from './schema.js'
import {DefinedInfo} from './defined-info.js'

/**
 * @typedef {import('./schema.js').Properties} Properties
 * @typedef {import('./schema.js').Normal} Normal
 * @typedef {import('./info.js').Info} Info
 */

/**
 * @typedef {Object.<string, string>} Attributes
 *
 * @typedef {Object} Definition
 * @property {Object.<string, number|null>} properties
 * @property {(attributes: Attributes, property: string) => string} transform
 * @property {string} [space]
 * @property {Attributes} [attributes]
 * @property {Array.<string>} [mustUseProperty]
 */

var own = {}.hasOwnProperty

/**
 * @param {Definition} definition
 * @returns {import('./schema.js').Schema}
 */
export function create(definition) {
  /** @type {Properties} */
  var property = {}
  /** @type {Normal} */
  var normal = {}
  /** @type {string} */
  var prop
  /** @type {Info} */
  var info

  for (prop in definition.properties) {
    if (own.call(definition.properties, prop)) {
      info = new DefinedInfo(
        prop,
        definition.transform(definition.attributes, prop),
        definition.properties[prop],
        definition.space
      )

      if (
        definition.mustUseProperty &&
        definition.mustUseProperty.includes(prop)
      ) {
        info.mustUseProperty = true
      }

      property[prop] = info

      normal[normalize(prop)] = prop
      normal[normalize(info.attribute)] = prop
    }
  }

  return new Schema(property, normal, definition.space)
}
