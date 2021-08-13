import {caseSensitiveTransform} from './case-sensitive-transform.js'

/**
 * @param {Object.<string, string>} attributes
 * @param {string} property
 * @returns {string}
 */
export function caseInsensitiveTransform(attributes, property) {
  return caseSensitiveTransform(attributes, property.toLowerCase())
}
