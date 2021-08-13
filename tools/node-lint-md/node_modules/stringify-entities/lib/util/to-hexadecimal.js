/**
 * Transform `code` into a hexadecimal character reference.
 *
 * @param {number} code
 * @param {number} next
 * @param {boolean} omit
 * @returns {string}
 */
export function toHexadecimal(code, next, omit) {
  var value = '&#x' + code.toString(16).toUpperCase()
  return omit && next && !/[\dA-Fa-f]/.test(String.fromCharCode(next))
    ? value
    : value + ';'
}
