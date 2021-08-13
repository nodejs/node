/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

import assert from 'assert'
import {decodeEntity} from 'parse-entities/decode-entity.js'
import {
  asciiAlphanumeric,
  asciiDigit,
  asciiHexDigit
} from 'micromark-util-character'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'

/** @type {Construct} */
export const characterReference = {
  name: 'characterReference',
  tokenize: tokenizeCharacterReference
}

/** @type {Tokenizer} */
function tokenizeCharacterReference(effects, ok, nok) {
  const self = this
  let size = 0
  /** @type {number} */
  let max
  /** @type {(code: Code) => code is number} */
  let test

  return start

  /** @type {State} */
  function start(code) {
    assert(code === codes.ampersand, 'expected `&`')
    effects.enter(types.characterReference)
    effects.enter(types.characterReferenceMarker)
    effects.consume(code)
    effects.exit(types.characterReferenceMarker)
    return open
  }

  /** @type {State} */
  function open(code) {
    if (code === codes.numberSign) {
      effects.enter(types.characterReferenceMarkerNumeric)
      effects.consume(code)
      effects.exit(types.characterReferenceMarkerNumeric)
      return numeric
    }

    effects.enter(types.characterReferenceValue)
    max = constants.characterReferenceNamedSizeMax
    test = asciiAlphanumeric
    return value(code)
  }

  /** @type {State} */
  function numeric(code) {
    if (code === codes.uppercaseX || code === codes.lowercaseX) {
      effects.enter(types.characterReferenceMarkerHexadecimal)
      effects.consume(code)
      effects.exit(types.characterReferenceMarkerHexadecimal)
      effects.enter(types.characterReferenceValue)
      max = constants.characterReferenceHexadecimalSizeMax
      test = asciiHexDigit
      return value
    }

    effects.enter(types.characterReferenceValue)
    max = constants.characterReferenceDecimalSizeMax
    test = asciiDigit
    return value(code)
  }

  /** @type {State} */
  function value(code) {
    /** @type {Token} */
    let token

    if (code === codes.semicolon && size) {
      token = effects.exit(types.characterReferenceValue)

      if (
        test === asciiAlphanumeric &&
        !decodeEntity(self.sliceSerialize(token))
      ) {
        return nok(code)
      }

      effects.enter(types.characterReferenceMarker)
      effects.consume(code)
      effects.exit(types.characterReferenceMarker)
      effects.exit(types.characterReference)
      return ok
    }

    if (test(code) && size++ < max) {
      effects.consume(code)
      return value
    }

    return nok(code)
  }
}
