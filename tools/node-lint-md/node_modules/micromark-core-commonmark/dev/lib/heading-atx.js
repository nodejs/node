/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 */

import assert from 'assert'
import {factorySpace} from 'micromark-factory-space'
import {
  markdownLineEnding,
  markdownLineEndingOrSpace,
  markdownSpace
} from 'micromark-util-character'
import {splice} from 'micromark-util-chunked'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'

/** @type {Construct} */
export const headingAtx = {
  name: 'headingAtx',
  tokenize: tokenizeHeadingAtx,
  resolve: resolveHeadingAtx
}

/** @type {Resolver} */
function resolveHeadingAtx(events, context) {
  let contentEnd = events.length - 2
  let contentStart = 3
  /** @type {Token} */
  let content
  /** @type {Token} */
  let text

  // Prefix whitespace, part of the opening.
  if (events[contentStart][1].type === types.whitespace) {
    contentStart += 2
  }

  // Suffix whitespace, part of the closing.
  if (
    contentEnd - 2 > contentStart &&
    events[contentEnd][1].type === types.whitespace
  ) {
    contentEnd -= 2
  }

  if (
    events[contentEnd][1].type === types.atxHeadingSequence &&
    (contentStart === contentEnd - 1 ||
      (contentEnd - 4 > contentStart &&
        events[contentEnd - 2][1].type === types.whitespace))
  ) {
    contentEnd -= contentStart + 1 === contentEnd ? 2 : 4
  }

  if (contentEnd > contentStart) {
    content = {
      type: types.atxHeadingText,
      start: events[contentStart][1].start,
      end: events[contentEnd][1].end
    }
    text = {
      type: types.chunkText,
      start: events[contentStart][1].start,
      end: events[contentEnd][1].end,
      // @ts-expect-error Constants are fine to assign.
      contentType: constants.contentTypeText
    }

    splice(events, contentStart, contentEnd - contentStart + 1, [
      ['enter', content, context],
      ['enter', text, context],
      ['exit', text, context],
      ['exit', content, context]
    ])
  }

  return events
}

/** @type {Tokenizer} */
function tokenizeHeadingAtx(effects, ok, nok) {
  const self = this
  let size = 0

  return start

  /** @type {State} */
  function start(code) {
    assert(code === codes.numberSign, 'expected `#`')
    effects.enter(types.atxHeading)
    effects.enter(types.atxHeadingSequence)
    return fenceOpenInside(code)
  }

  /** @type {State} */
  function fenceOpenInside(code) {
    if (
      code === codes.numberSign &&
      size++ < constants.atxHeadingOpeningFenceSizeMax
    ) {
      effects.consume(code)
      return fenceOpenInside
    }

    if (code === codes.eof || markdownLineEndingOrSpace(code)) {
      effects.exit(types.atxHeadingSequence)
      return self.interrupt ? ok(code) : headingBreak(code)
    }

    return nok(code)
  }

  /** @type {State} */
  function headingBreak(code) {
    if (code === codes.numberSign) {
      effects.enter(types.atxHeadingSequence)
      return sequence(code)
    }

    if (code === codes.eof || markdownLineEnding(code)) {
      effects.exit(types.atxHeading)
      return ok(code)
    }

    if (markdownSpace(code)) {
      return factorySpace(effects, headingBreak, types.whitespace)(code)
    }

    effects.enter(types.atxHeadingText)
    return data(code)
  }

  /** @type {State} */
  function sequence(code) {
    if (code === codes.numberSign) {
      effects.consume(code)
      return sequence
    }

    effects.exit(types.atxHeadingSequence)
    return headingBreak(code)
  }

  /** @type {State} */
  function data(code) {
    if (
      code === codes.eof ||
      code === codes.numberSign ||
      markdownLineEndingOrSpace(code)
    ) {
      effects.exit(types.atxHeadingText)
      return headingBreak(code)
    }

    effects.consume(code)
    return data
  }
}
