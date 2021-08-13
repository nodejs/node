/**
 * @typedef {Object} CoreOptions
 * @property {string[]} [subset=[]] Whether to only escape the given subset of characters (`string[]`)
 * @property {boolean} [escapeOnly=false] Whether to only escape possibly dangerous characters (`boolean`, default: `false`). Those characters are `"`, `&`, `'`, `<`, `>`, and `` ` ``
 *
 * @typedef {Object} FormatOptions
 * @property {function (number, number, CoreWithFormatOptions): string} format
 *
 * @typedef {CoreOptions & FormatOptions & import('./util/format-smart.js').FormatSmartOptions} CoreWithFormatOptions
 */

/**
 * Encode special characters in `value`.
 *
 * @param {string} value
 * @param {CoreWithFormatOptions} [options]
 * @returns {string}
 */
export function core(value, options) {
  value = value.replace(
    options.subset ? charactersToExpression(options.subset) : /["&'<>`]/g,
    basic
  )

  if (options.subset || options.escapeOnly) {
    return value
  }

  return (
    value
      // Surrogate pairs.
      .replace(/[\uD800-\uDBFF][\uDC00-\uDFFF]/g, surrogate)
      // BMP control characters (C0 except for LF, CR, SP; DEL; and some more
      // non-ASCII ones).
      .replace(
        // eslint-disable-next-line no-control-regex, unicorn/no-hex-escape
        /[\x01-\t\v\f\x0E-\x1F\x7F\x81\x8D\x8F\x90\x9D\xA0-\uFFFF]/g,
        basic
      )
  )

  /**
   * @param {string} pair
   * @param {number} index
   * @param {string} all
   */
  function surrogate(pair, index, all) {
    return options.format(
      (pair.charCodeAt(0) - 0xd800) * 0x400 +
        pair.charCodeAt(1) -
        0xdc00 +
        0x10000,
      all.charCodeAt(index + 2),
      options
    )
  }

  /**
   * @param {string} character
   * @param {number} index
   * @param {string} all
   */
  function basic(character, index, all) {
    return options.format(
      character.charCodeAt(0),
      all.charCodeAt(index + 1),
      options
    )
  }
}

/**
 * @param {string[]} subset
 * @returns {RegExp}
 */
function charactersToExpression(subset) {
  /** @type {string[]} */
  var groups = []
  var index = -1

  while (++index < subset.length) {
    groups.push(subset[index].replace(/[|\\{}()[\]^$+*?.]/g, '\\$&'))
  }

  return new RegExp('(?:' + groups.join('|') + ')', 'g')
}
