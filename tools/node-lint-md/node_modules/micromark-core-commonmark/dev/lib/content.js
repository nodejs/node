/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 */

import assert from 'assert'
import {factorySpace} from 'micromark-factory-space'
import {markdownLineEnding} from 'micromark-util-character'
import {subtokenize} from 'micromark-util-subtokenize'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'

/**
 * No name because it must not be turned off.
 * @type {Construct}
 */
export const content = {tokenize: tokenizeContent, resolve: resolveContent}

/** @type {Construct} */
const continuationConstruct = {tokenize: tokenizeContinuation, partial: true}

/**
 * Content is transparent: it’s parsed right now. That way, definitions are also
 * parsed right now: before text in paragraphs (specifically, media) are parsed.
 *
 * @type {Resolver}
 */
function resolveContent(events) {
  subtokenize(events)
  return events
}

/** @type {Tokenizer} */
function tokenizeContent(effects, ok) {
  /** @type {Token} */
  let previous

  return start

  /** @type {State} */
  function start(code) {
    assert(
      code !== codes.eof && !markdownLineEnding(code),
      'expected no eof or eol'
    )

    effects.enter(types.content)
    previous = effects.enter(types.chunkContent, {
      contentType: constants.contentTypeContent
    })
    return data(code)
  }

  /** @type {State} */
  function data(code) {
    if (code === codes.eof) {
      return contentEnd(code)
    }

    if (markdownLineEnding(code)) {
      return effects.check(
        continuationConstruct,
        contentContinue,
        contentEnd
      )(code)
    }

    // Data.
    effects.consume(code)
    return data
  }

  /** @type {State} */
  function contentEnd(code) {
    effects.exit(types.chunkContent)
    effects.exit(types.content)
    return ok(code)
  }

  /** @type {State} */
  function contentContinue(code) {
    assert(markdownLineEnding(code), 'expected eol')
    effects.consume(code)
    effects.exit(types.chunkContent)
    previous.next = effects.enter(types.chunkContent, {
      contentType: constants.contentTypeContent,
      previous
    })
    previous = previous.next
    return data
  }
}

/** @type {Tokenizer} */
function tokenizeContinuation(effects, ok, nok) {
  const self = this

  return startLookahead

  /** @type {State} */
  function startLookahead(code) {
    assert(markdownLineEnding(code), 'expected a line ending')
    effects.exit(types.chunkContent)
    effects.enter(types.lineEnding)
    effects.consume(code)
    effects.exit(types.lineEnding)
    return factorySpace(effects, prefixed, types.linePrefix)
  }

  /** @type {State} */
  function prefixed(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      return nok(code)
    }

    const tail = self.events[self.events.length - 1]

    if (
      !self.parser.constructs.disable.null.includes('codeIndented') &&
      tail &&
      tail[1].type === types.linePrefix &&
      tail[2].sliceSerialize(tail[1], true).length >= constants.tabSize
    ) {
      return ok(code)
    }

    return effects.interrupt(self.parser.constructs.flow, nok, ok)(code)
  }
}
