import {characterEntitiesLegacy} from 'character-entities-legacy'
import {characterReferenceInvalid} from 'character-reference-invalid'
import {isDecimal} from 'is-decimal'
import {isHexadecimal} from 'is-hexadecimal'
import {isAlphanumerical} from 'is-alphanumerical'
import {decodeEntity} from './decode-entity.js'

/**
 * @template {typeof globalThis} WarningContext
 * @template {typeof globalThis} ReferenceContext
 * @template {typeof globalThis} TextContext
 * @typedef {Object} ParseEntitiesOptions
 * @property {string} [additional=''] Additional character to accept. This allows other characters, without error, when following an ampersand
 * @property {boolean} [attribute=false] Whether to parse `value` as an attribute value
 * @property {boolean} [nonTerminated=true] Whether to allow non-terminated entities. For example, `&copycat` for `©cat`. This behaviour is spec-compliant but can lead to unexpected results
 * @property {Position | Point} [position] Starting `position` of `value` (`Point` or `Position`). Useful when dealing with values nested in some sort of syntax tree
 * @property {WarningContext} warningContext Context used when calling `warning`
 * @property {WarningHandler<WarningContext>} warning Warning handler
 * @property {ReferenceContext} referenceContext Context used when calling `reference`
 * @property {ReferenceHandler<ReferenceContext>} reference Reference handler
 * @property {TextContext} textContext Context used when calling `text`
 * @property {TextHandler<TextContext>} text Text handler
 */

/**
 * @typedef {Object} Position
 * @property {Point} start
 * @property {Point} [end]
 * @property {number[]} [indent]
 */

/**
 * @typedef {Object} Point
 * @property {number} line
 * @property {number} column
 * @property {number} offset
 */

/**
 * @template {typeof globalThis} Context
 * @callback WarningHandler
 * @this {Context} `this` refers to `warningContext` given to `parseEntities`
 * @param {string} reason Human-readable reason for triggering a parse error.
 * @param {Point} point Place at which the parse error occurred.
 * @param {number} code Identifier of reason for triggering a parse error.
 * @returns {void}
 */

/**
 * @template {typeof globalThis} Context
 * @callback ReferenceHandler
 * @this {Context} `this` refers to `referenceContext` given to `parseEntities`.
 * @param {string} value String of content.
 * @param {Position} position Place at which `value` starts and ends.
 * @param {string} source Source of character reference.
 * @returns {void}
 */

/**
 * @template {typeof globalThis} Context
 * @callback TextHandler
 * @this {Context} `this` refers to `textContext` given to `parseEntities`.
 * @param {string} value String of content.
 * @param {Position} position Place at which `value` starts and ends.
 * @returns {void}
 */

var own = {}.hasOwnProperty
var fromCharCode = String.fromCharCode

// Warning messages.
var messages = [
  undefined,
  /* 1: Non terminated (named) */
  'Named character references must be terminated by a semicolon',
  /* 2: Non terminated (numeric) */
  'Numeric character references must be terminated by a semicolon',
  /* 3: Empty (named) */
  'Named character references cannot be empty',
  /* 4: Empty (numeric) */
  'Numeric character references cannot be empty',
  /* 5: Unknown (named) */
  'Named character references must be known',
  /* 6: Disallowed (numeric) */
  'Numeric character references cannot be disallowed',
  /* 7: Prohibited (numeric) */
  'Numeric character references cannot be outside the permissible Unicode range'
]

/**
 * Parse entities.
 *
 * @template {typeof globalThis} WarningContext
 * @template {typeof globalThis} ReferenceContext
 * @template {typeof globalThis} TextContext
 * @param {string} value
 * @param {Partial<ParseEntitiesOptions<WarningContext, ReferenceContext, TextContext>>} [options={}]
 */
export function parseEntities(value, options = {}) {
  var additional =
    typeof options.additional === 'string'
      ? options.additional.charCodeAt(0)
      : options.additional
  var index = 0
  var lines = -1
  var queue = ''
  /** @type {string[]} */
  var result = []
  /** @type {Point?} */
  var pos
  /** @type {number[]?} */
  var indent
  /** @type {number} */
  var line
  /** @type {number} */
  var column
  /** @type {string} */
  var entityCharacters
  /** @type {string|false} */
  var namedEntity
  /** @type {boolean} */
  var terminated
  /** @type {string} */
  var characters
  /** @type {number} */
  var character
  /** @type {string} */
  var reference
  /** @type {number} */
  var referenceCode
  /** @type {number} */
  var following
  /** @type {number} */
  var reason
  /** @type {string} */
  var output
  /** @type {string} */
  var entity
  /** @type {number} */
  var begin
  /** @type {number} */
  var start
  /** @type {string} */
  var type
  /** @type {(code: number) => boolean} */
  var test
  /** @type {Point} */
  var previous
  /** @type {Point} */
  var next
  /** @type {number} */
  var diff
  /** @type {number} */
  var end

  if (options.position) {
    if ('start' in options.position || 'indent' in options.position) {
      indent = options.position.indent
      pos = options.position.start
    } else {
      pos = options.position
    }
  }

  line = (pos && pos.line) || 1
  column = (pos && pos.column) || 1

  // Cache the current point.
  previous = now()

  // Ensure the algorithm walks over the first character (inclusive).
  index--

  while (++index <= value.length) {
    // If the previous character was a newline.
    if (character === 10 /* `\n` */) {
      column = (indent && indent[lines]) || 1
    }

    character = value.charCodeAt(index)

    if (character === 38 /* `&` */) {
      following = value.charCodeAt(index + 1)

      // The behavior depends on the identity of the next character.
      if (
        following === 9 /* `\t` */ ||
        following === 10 /* `\n` */ ||
        following === 12 /* `\f` */ ||
        following === 32 /* ` ` */ ||
        following === 38 /* `&` */ ||
        following === 60 /* `<` */ ||
        Number.isNaN(following) ||
        (additional && following === additional)
      ) {
        // Not a character reference.
        // No characters are consumed, and nothing is returned.
        // This is not an error, either.
        queue += fromCharCode(character)
        column++
        continue
      }

      start = index + 1
      begin = start
      end = start

      if (following === 35 /* `#` */) {
        // Numerical reference.
        end = ++begin

        // The behavior further depends on the next character.
        following = value.charCodeAt(end)

        if (following === 88 /* `X` */ || following === 120 /* `x` */) {
          // ASCII hexadecimal digits.
          type = 'hexadecimal'
          end = ++begin
        } else {
          // ASCII decimal digits.
          type = 'decimal'
        }
      } else {
        // Named entity.
        type = 'named'
      }

      entityCharacters = ''
      entity = ''
      characters = ''
      // Each type of character reference accepts different characters.
      // This test is used to detect whether a reference has ended (as the semicolon
      // is not strictly needed).
      test =
        type === 'named'
          ? isAlphanumerical
          : type === 'decimal'
          ? isDecimal
          : isHexadecimal

      end--

      while (++end <= value.length) {
        following = value.charCodeAt(end)

        if (!test(following)) {
          break
        }

        characters += fromCharCode(following)

        // Check if we can match a legacy named reference.
        // If so, we cache that as the last viable named reference.
        // This ensures we do not need to walk backwards later.
        if (type === 'named' && own.call(characterEntitiesLegacy, characters)) {
          entityCharacters = characters
          entity = characterEntitiesLegacy[characters]
        }
      }

      terminated = value.charCodeAt(end) === 59 /* `;` */

      if (terminated) {
        end++

        namedEntity = type === 'named' ? decodeEntity(characters) : false

        if (namedEntity) {
          entityCharacters = characters
          entity = namedEntity
        }
      }

      diff = 1 + end - start

      if (!terminated && options.nonTerminated === false) {
        // Empty.
      } else if (!characters) {
        // An empty (possible) reference is valid, unless it’s numeric (thus an
        // ampersand followed by an octothorp).
        if (type !== 'named') {
          warning(4 /* Empty (numeric) */, diff)
        }
      } else if (type === 'named') {
        // An ampersand followed by anything unknown, and not terminated, is
        // invalid.
        if (terminated && !entity) {
          warning(5 /* Unknown (named) */, 1)
        } else {
          // If theres something after an entity name which is not known, cap
          // the reference.
          if (entityCharacters !== characters) {
            end = begin + entityCharacters.length
            diff = 1 + end - begin
            terminated = false
          }

          // If the reference is not terminated, warn.
          if (!terminated) {
            reason = entityCharacters
              ? 1 /* Non terminated (named) */
              : 3 /* Empty (named) */

            if (options.attribute) {
              following = value.charCodeAt(end)

              if (following === 61 /* `=` */) {
                warning(reason, diff)
                entity = null
              } else if (isAlphanumerical(following)) {
                entity = null
              } else {
                warning(reason, diff)
              }
            } else {
              warning(reason, diff)
            }
          }
        }

        reference = entity
      } else {
        if (!terminated) {
          // All non-terminated numeric references are not rendered, and emit a
          // warning.
          warning(2 /* Non terminated (numeric) */, diff)
        }

        // When terminated and numerical, parse as either hexadecimal or
        // decimal.
        referenceCode = Number.parseInt(
          characters,
          type === 'hexadecimal' ? 16 : 10
        )

        // Emit a warning when the parsed number is prohibited, and replace with
        // replacement character.
        if (prohibited(referenceCode)) {
          warning(7 /* Prohibited (numeric) */, diff)
          reference = fromCharCode(65533 /* `�` */)
        } else if (referenceCode in characterReferenceInvalid) {
          // Emit a warning when the parsed number is disallowed, and replace by
          // an alternative.
          warning(6 /* Disallowed (numeric) */, diff)
          reference = characterReferenceInvalid[referenceCode]
        } else {
          // Parse the number.
          output = ''

          // Emit a warning when the parsed number should not be used.
          if (disallowed(referenceCode)) {
            warning(6 /* Disallowed (numeric) */, diff)
          }

          // Serialize the number.
          if (referenceCode > 0xffff) {
            referenceCode -= 0x10000
            output += fromCharCode((referenceCode >>> (10 & 0x3ff)) | 0xd800)
            referenceCode = 0xdc00 | (referenceCode & 0x3ff)
          }

          reference = output + fromCharCode(referenceCode)
        }
      }

      // Found it!
      // First eat the queued characters as normal text, then eat a reference.
      if (reference) {
        flush()

        previous = now()
        index = end - 1
        column += end - start + 1
        result.push(reference)
        next = now()
        next.offset++

        if (options.reference) {
          options.reference.call(
            options.referenceContext,
            reference,
            {start: previous, end: next},
            value.slice(start - 1, end)
          )
        }

        previous = next
      } else {
        // If we could not find a reference, queue the checked characters (as
        // normal characters), and move the pointer to their end.
        // This is possible because we can be certain neither newlines nor
        // ampersands are included.
        characters = value.slice(start - 1, end)
        queue += characters
        column += characters.length
        index = end - 1
      }
    } else {
      // Handle anything other than an ampersand, including newlines and EOF.
      if (character === 10 /* `\n` */) {
        line++
        lines++
        column = 0
      }

      if (Number.isNaN(character)) {
        flush()
      } else {
        queue += fromCharCode(character)
        column++
      }
    }
  }

  // Return the reduced nodes.
  return result.join('')

  // Get current position.
  function now() {
    return {
      line,
      column,
      offset: index + ((pos && pos.offset) || 0)
    }
  }

  /**
   * Handle the warning.
   *
   * @param {number} code
   * @param {number} offset
   */
  function warning(code, offset) {
    /** @type {Point} */
    var position

    if (options.warning) {
      position = now()
      position.column += offset
      position.offset += offset

      options.warning.call(
        options.warningContext,
        messages[code],
        position,
        code
      )
    }
  }

  /**
   * Flush `queue` (normal text).
   * Macro invoked before each reference and at the end of `value`.
   * Does nothing when `queue` is empty.
   */
  function flush() {
    if (queue) {
      result.push(queue)

      if (options.text) {
        options.text.call(options.textContext, queue, {
          start: previous,
          end: now()
        })
      }

      queue = ''
    }
  }
}

/**
 * Check if `character` is outside the permissible unicode range.
 *
 * @param {number} code
 * @returns {boolean}
 */
function prohibited(code) {
  return (code >= 0xd800 && code <= 0xdfff) || code > 0x10ffff
}

/**
 * Check if `character` is disallowed.
 *
 * @param {number} code
 * @returns {boolean}
 */
function disallowed(code) {
  return (
    (code >= 0x0001 && code <= 0x0008) ||
    code === 0x000b ||
    (code >= 0x000d && code <= 0x001f) ||
    (code >= 0x007f && code <= 0x009f) ||
    (code >= 0xfdd0 && code <= 0xfdef) ||
    (code & 0xffff) === 0xffff ||
    (code & 0xffff) === 0xfffe
  )
}
