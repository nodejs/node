/**
 * @typedef {Object} FormatSmartOptions
 * @property {boolean} [useNamedReferences=false] Prefer named character references (`&amp;`) where possible (`boolean?`, default: `false`)
 * @property {boolean} [useShortestReferences=false] Prefer the shortest possible reference, if that results in less bytes (`boolean?`, default: `false`). **Note**: `useNamedReferences` can be omitted when using `useShortestReferences`
 * @property {boolean} [omitOptionalSemicolons=false] Whether to omit semicolons when possible (`boolean?`, default: `false`). **Note**: This creates what HTML calls “parse errors” but is otherwise still valid HTML — don’t use this except when building a minifier. Omitting semicolons is possible for legacy named references in certain cases, and numeric references in some cases
 * @property {boolean} [attribute=false] Only needed when operating dangerously with `omitOptionalSemicolons: true`. Create character references which don’t fail in attributes (`boolean?`, default: `false`).
 */

import {toHexadecimal} from './to-hexadecimal.js'
import {toDecimal} from './to-decimal.js'
import {toNamed} from './to-named.js'

/**
 * Encode `character` according to `options`.
 *
 * @param {number} code
 * @param {number} next
 * @param {FormatSmartOptions} options
 * @returns {string}
 */
export function formatSmart(code, next, options) {
  /** @type {string} */
  var named
  /** @type {string} */
  var numeric
  /** @type {string} */
  var decimal

  if (options.useNamedReferences || options.useShortestReferences) {
    named = toNamed(
      code,
      next,
      options.omitOptionalSemicolons,
      options.attribute
    )
  }

  if (options.useShortestReferences || !named) {
    numeric = toHexadecimal(code, next, options.omitOptionalSemicolons)

    // Use the shortest numeric reference when requested.
    // A simple algorithm would use decimal for all code points under 100, as
    // those are shorter than hexadecimal:
    //
    // * `&#99;` vs `&#x63;` (decimal shorter)
    // * `&#100;` vs `&#x64;` (equal)
    //
    // However, because we take `next` into consideration when `omit` is used,
    // And it would be possible that decimals are shorter on bigger values as
    // well if `next` is hexadecimal but not decimal, we instead compare both.
    if (options.useShortestReferences) {
      decimal = toDecimal(code, next, options.omitOptionalSemicolons)

      if (decimal.length < numeric.length) {
        numeric = decimal
      }
    }
  }

  return named &&
    (!options.useShortestReferences || named.length < numeric.length)
    ? named
    : numeric
}
