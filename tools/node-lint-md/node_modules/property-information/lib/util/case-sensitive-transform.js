/**
 * @param {Object.<string, string>} attributes
 * @param {string} attribute
 * @returns {string}
 */
export function caseSensitiveTransform(attributes, attribute) {
  return attribute in attributes ? attributes[attribute] : attribute
}
