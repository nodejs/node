/**
 * Transform `code` into a decimal character reference.
 *
 * @param {number} code
 * @param {number} next
 * @param {boolean} omit
 * @returns {string}
 */
export function toDecimal(code, next, omit) {
  var value = '&#' + String(code)
  return omit && next && !/\d/.test(String.fromCharCode(next))
    ? value
    : value + ';'
}
