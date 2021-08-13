/**
 * @typedef {Object} StringifyOptions
 * @property {boolean} [padLeft=true] Whether to pad a space before a token (`boolean`, default: `true`).
 * @property {boolean} [padRight=false] Whether to pad a space after a token (`boolean`, default: `false`).
 */

/**
 * Parse comma separated tokens to an array.
 *
 * @param {string} value
 * @returns {Array.<string>}
 */
export function parse(value) {
  /** @type {Array.<string>} */
  var tokens = []
  var input = String(value || '')
  var index = input.indexOf(',')
  var start = 0
  /** @type {boolean} */
  var end
  /** @type {string} */
  var token

  while (!end) {
    if (index === -1) {
      index = input.length
      end = true
    }

    token = input.slice(start, index).trim()

    if (token || !end) {
      tokens.push(token)
    }

    start = index + 1
    index = input.indexOf(',', start)
  }

  return tokens
}

/**
 * Serialize an array of strings to comma separated tokens.
 *
 * @param {Array.<string|number>} values
 * @param {StringifyOptions} [options]
 * @returns {string}
 */
export function stringify(values, options) {
  var settings = options || {}

  // Ensure the last empty entry is seen.
  if (values[values.length - 1] === '') {
    values = values.concat('')
  }

  return values
    .join(
      (settings.padRight ? ' ' : '') +
        ',' +
        (settings.padLeft === false ? '' : ' ')
    )
    .trim()
}
