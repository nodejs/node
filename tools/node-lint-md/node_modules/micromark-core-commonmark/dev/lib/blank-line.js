/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */

import {factorySpace} from 'micromark-factory-space'
import {markdownLineEnding} from 'micromark-util-character'
import {codes} from 'micromark-util-symbol/codes.js'
import {types} from 'micromark-util-symbol/types.js'

/** @type {Construct} */
export const blankLine = {tokenize: tokenizeBlankLine, partial: true}

/** @type {Tokenizer} */
function tokenizeBlankLine(effects, ok, nok) {
  return factorySpace(effects, afterWhitespace, types.linePrefix)

  /** @type {State} */
  function afterWhitespace(code) {
    return code === codes.eof || markdownLineEnding(code) ? ok(code) : nok(code)
  }
}
