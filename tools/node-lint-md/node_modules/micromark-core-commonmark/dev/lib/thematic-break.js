/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

import assert from 'assert'
import {factorySpace} from 'micromark-factory-space'
import {markdownLineEnding, markdownSpace} from 'micromark-util-character'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'

/** @type {Construct} */
export const thematicBreak = {
  name: 'thematicBreak',
  tokenize: tokenizeThematicBreak
}

/** @type {Tokenizer} */
function tokenizeThematicBreak(effects, ok, nok) {
  let size = 0
  /** @type {NonNullable<Code>} */
  let marker

  return start

  /** @type {State} */
  function start(code) {
    assert(
      code === codes.asterisk ||
        code === codes.dash ||
        code === codes.underscore,
      'expected `*`, `-`, or `_`'
    )

    effects.enter(types.thematicBreak)
    marker = code
    return atBreak(code)
  }

  /** @type {State} */
  function atBreak(code) {
    if (code === marker) {
      effects.enter(types.thematicBreakSequence)
      return sequence(code)
    }

    if (markdownSpace(code)) {
      return factorySpace(effects, atBreak, types.whitespace)(code)
    }

    if (
      size < constants.thematicBreakMarkerCountMin ||
      (code !== codes.eof && !markdownLineEnding(code))
    ) {
      return nok(code)
    }

    effects.exit(types.thematicBreak)
    return ok(code)
  }

  /** @type {State} */
  function sequence(code) {
    if (code === marker) {
      effects.consume(code)
      size++
      return sequence
    }

    effects.exit(types.thematicBreakSequence)
    return atBreak(code)
  }
}
