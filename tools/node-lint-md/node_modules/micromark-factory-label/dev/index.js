/**
 * @typedef {import('micromark-util-types').Effects} Effects
 * @typedef {import('micromark-util-types').TokenizeContext} TokenizeContext
 * @typedef {import('micromark-util-types').State} State
 */

import assert from 'assert'
import {markdownLineEnding, markdownSpace} from 'micromark-util-character'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'

/**
 * @this {TokenizeContext}
 * @param {Effects} effects
 * @param {State} ok
 * @param {State} nok
 * @param {string} type
 * @param {string} markerType
 * @param {string} stringType
 * @returns {State}
 */
// eslint-disable-next-line max-params
export function factoryLabel(effects, ok, nok, type, markerType, stringType) {
  const self = this
  let size = 0
  /** @type {boolean} */
  let data

  return start

  /** @type {State} */
  function start(code) {
    assert(code === codes.leftSquareBracket, 'expected `[`')
    effects.enter(type)
    effects.enter(markerType)
    effects.consume(code)
    effects.exit(markerType)
    effects.enter(stringType)
    return atBreak
  }

  /** @type {State} */
  function atBreak(code) {
    if (
      code === codes.eof ||
      code === codes.leftSquareBracket ||
      (code === codes.rightSquareBracket && !data) ||
      /* Hidden footnotes hook */
      /* c8 ignore next 3 */
      (code === codes.caret &&
        !size &&
        '_hiddenFootnoteSupport' in self.parser.constructs) ||
      size > constants.linkReferenceSizeMax
    ) {
      return nok(code)
    }

    if (code === codes.rightSquareBracket) {
      effects.exit(stringType)
      effects.enter(markerType)
      effects.consume(code)
      effects.exit(markerType)
      effects.exit(type)
      return ok
    }

    if (markdownLineEnding(code)) {
      effects.enter(types.lineEnding)
      effects.consume(code)
      effects.exit(types.lineEnding)
      return atBreak
    }

    effects.enter(types.chunkString, {contentType: constants.contentTypeString})
    return label(code)
  }

  /** @type {State} */
  function label(code) {
    if (
      code === codes.eof ||
      code === codes.leftSquareBracket ||
      code === codes.rightSquareBracket ||
      markdownLineEnding(code) ||
      size++ > constants.linkReferenceSizeMax
    ) {
      effects.exit(types.chunkString)
      return atBreak(code)
    }

    effects.consume(code)
    data = data || !markdownSpace(code)
    return code === codes.backslash ? labelEscape : label
  }

  /** @type {State} */
  function labelEscape(code) {
    if (
      code === codes.leftSquareBracket ||
      code === codes.backslash ||
      code === codes.rightSquareBracket
    ) {
      effects.consume(code)
      size++
      return label
    }

    return label(code)
  }
}
