/**
 * @param {number} code
 * @returns {string}
 */
export function formatBasic(code) {
  return '&#x' + code.toString(16).toUpperCase() + ';'
}
