/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 */

import {factorySpace} from 'micromark-factory-space'
import {markdownLineEnding} from 'micromark-util-character'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'

/** @type {Construct} */
export const codeIndented = {
  name: 'codeIndented',
  tokenize: tokenizeCodeIndented
}

/** @type {Construct} */
const indentedContent = {tokenize: tokenizeIndentedContent, partial: true}

/** @type {Tokenizer} */
function tokenizeCodeIndented(effects, ok, nok) {
  const self = this
  return start

  /** @type {State} */
  function start(code) {
    effects.enter(types.codeIndented)
    return factorySpace(
      effects,
      afterStartPrefix,
      types.linePrefix,
      constants.tabSize + 1
    )(code)
  }

  /** @type {State} */
  function afterStartPrefix(code) {
    const tail = self.events[self.events.length - 1]
    return tail &&
      tail[1].type === types.linePrefix &&
      tail[2].sliceSerialize(tail[1], true).length >= constants.tabSize
      ? afterPrefix(code)
      : nok(code)
  }

  /** @type {State} */
  function afterPrefix(code) {
    if (code === codes.eof) {
      return after(code)
    }

    if (markdownLineEnding(code)) {
      return effects.attempt(indentedContent, afterPrefix, after)(code)
    }

    effects.enter(types.codeFlowValue)
    return content(code)
  }

  /** @type {State} */
  function content(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      effects.exit(types.codeFlowValue)
      return afterPrefix(code)
    }

    effects.consume(code)
    return content
  }

  /** @type {State} */
  function after(code) {
    effects.exit(types.codeIndented)
    return ok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeIndentedContent(effects, ok, nok) {
  const self = this

  return start

  /** @type {State} */
  function start(code) {
    // If this is a lazy line, it can’t be code.
    if (self.parser.lazy[self.now().line]) {
      return nok(code)
    }

    if (markdownLineEnding(code)) {
      effects.enter(types.lineEnding)
      effects.consume(code)
      effects.exit(types.lineEnding)
      return start
    }

    return factorySpace(
      effects,
      afterPrefix,
      types.linePrefix,
      constants.tabSize + 1
    )(code)
  }

  /** @type {State} */
  function afterPrefix(code) {
    const tail = self.events[self.events.length - 1]
    return tail &&
      tail[1].type === types.linePrefix &&
      tail[2].sliceSerialize(tail[1], true).length >= constants.tabSize
      ? ok(code)
      : markdownLineEnding(code)
      ? start(code)
      : nok(code)
  }
}
