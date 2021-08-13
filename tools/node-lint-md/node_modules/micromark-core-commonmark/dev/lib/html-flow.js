/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

import assert from 'assert'
import {
  asciiAlpha,
  asciiAlphanumeric,
  markdownLineEnding,
  markdownLineEndingOrSpace,
  markdownSpace
} from 'micromark-util-character'
import {htmlBlockNames, htmlRawNames} from 'micromark-util-html-tag-name'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'
import {blankLine} from './blank-line.js'

/** @type {Construct} */
export const htmlFlow = {
  name: 'htmlFlow',
  tokenize: tokenizeHtmlFlow,
  resolveTo: resolveToHtmlFlow,
  concrete: true
}

/** @type {Construct} */
const nextBlankConstruct = {tokenize: tokenizeNextBlank, partial: true}

/** @type {Resolver} */
function resolveToHtmlFlow(events) {
  let index = events.length

  while (index--) {
    if (
      events[index][0] === 'enter' &&
      events[index][1].type === types.htmlFlow
    ) {
      break
    }
  }

  if (index > 1 && events[index - 2][1].type === types.linePrefix) {
    // Add the prefix start to the HTML token.
    events[index][1].start = events[index - 2][1].start
    // Add the prefix start to the HTML line token.
    events[index + 1][1].start = events[index - 2][1].start
    // Remove the line prefix.
    events.splice(index - 2, 2)
  }

  return events
}

/** @type {Tokenizer} */
function tokenizeHtmlFlow(effects, ok, nok) {
  const self = this
  /** @type {number} */
  let kind
  /** @type {boolean} */
  let startTag
  /** @type {string} */
  let buffer
  /** @type {number} */
  let index
  /** @type {Code} */
  let marker

  return start

  /** @type {State} */
  function start(code) {
    assert(code === codes.lessThan, 'expected `<`')
    effects.enter(types.htmlFlow)
    effects.enter(types.htmlFlowData)
    effects.consume(code)
    return open
  }

  /** @type {State} */
  function open(code) {
    if (code === codes.exclamationMark) {
      effects.consume(code)
      return declarationStart
    }

    if (code === codes.slash) {
      effects.consume(code)
      return tagCloseStart
    }

    if (code === codes.questionMark) {
      effects.consume(code)
      kind = constants.htmlInstruction
      // While we’re in an instruction instead of a declaration, we’re on a `?`
      // right now, so we do need to search for `>`, similar to declarations.
      return self.interrupt ? ok : continuationDeclarationInside
    }

    if (asciiAlpha(code)) {
      effects.consume(code)
      buffer = String.fromCharCode(code)
      startTag = true
      return tagName
    }

    return nok(code)
  }

  /** @type {State} */
  function declarationStart(code) {
    if (code === codes.dash) {
      effects.consume(code)
      kind = constants.htmlComment
      return commentOpenInside
    }

    if (code === codes.leftSquareBracket) {
      effects.consume(code)
      kind = constants.htmlCdata
      buffer = constants.cdataOpeningString
      index = 0
      return cdataOpenInside
    }

    if (asciiAlpha(code)) {
      effects.consume(code)
      kind = constants.htmlDeclaration
      return self.interrupt ? ok : continuationDeclarationInside
    }

    return nok(code)
  }

  /** @type {State} */
  function commentOpenInside(code) {
    if (code === codes.dash) {
      effects.consume(code)
      return self.interrupt ? ok : continuationDeclarationInside
    }

    return nok(code)
  }

  /** @type {State} */
  function cdataOpenInside(code) {
    if (code === buffer.charCodeAt(index++)) {
      effects.consume(code)
      return index === buffer.length
        ? self.interrupt
          ? ok
          : continuation
        : cdataOpenInside
    }

    return nok(code)
  }

  /** @type {State} */
  function tagCloseStart(code) {
    if (asciiAlpha(code)) {
      effects.consume(code)
      buffer = String.fromCharCode(code)
      return tagName
    }

    return nok(code)
  }

  /** @type {State} */
  function tagName(code) {
    if (
      code === codes.eof ||
      code === codes.slash ||
      code === codes.greaterThan ||
      markdownLineEndingOrSpace(code)
    ) {
      if (
        code !== codes.slash &&
        startTag &&
        htmlRawNames.includes(buffer.toLowerCase())
      ) {
        kind = constants.htmlRaw
        return self.interrupt ? ok(code) : continuation(code)
      }

      if (htmlBlockNames.includes(buffer.toLowerCase())) {
        kind = constants.htmlBasic

        if (code === codes.slash) {
          effects.consume(code)
          return basicSelfClosing
        }

        return self.interrupt ? ok(code) : continuation(code)
      }

      kind = constants.htmlComplete
      // Do not support complete HTML when interrupting
      return self.interrupt && !self.parser.lazy[self.now().line]
        ? nok(code)
        : startTag
        ? completeAttributeNameBefore(code)
        : completeClosingTagAfter(code)
    }

    if (code === codes.dash || asciiAlphanumeric(code)) {
      effects.consume(code)
      buffer += String.fromCharCode(code)
      return tagName
    }

    return nok(code)
  }

  /** @type {State} */
  function basicSelfClosing(code) {
    if (code === codes.greaterThan) {
      effects.consume(code)
      return self.interrupt ? ok : continuation
    }

    return nok(code)
  }

  /** @type {State} */
  function completeClosingTagAfter(code) {
    if (markdownSpace(code)) {
      effects.consume(code)
      return completeClosingTagAfter
    }

    return completeEnd(code)
  }

  /** @type {State} */
  function completeAttributeNameBefore(code) {
    if (code === codes.slash) {
      effects.consume(code)
      return completeEnd
    }

    if (code === codes.colon || code === codes.underscore || asciiAlpha(code)) {
      effects.consume(code)
      return completeAttributeName
    }

    if (markdownSpace(code)) {
      effects.consume(code)
      return completeAttributeNameBefore
    }

    return completeEnd(code)
  }

  /** @type {State} */
  function completeAttributeName(code) {
    if (
      code === codes.dash ||
      code === codes.dot ||
      code === codes.colon ||
      code === codes.underscore ||
      asciiAlphanumeric(code)
    ) {
      effects.consume(code)
      return completeAttributeName
    }

    return completeAttributeNameAfter(code)
  }

  /** @type {State} */
  function completeAttributeNameAfter(code) {
    if (code === codes.equalsTo) {
      effects.consume(code)
      return completeAttributeValueBefore
    }

    if (markdownSpace(code)) {
      effects.consume(code)
      return completeAttributeNameAfter
    }

    return completeAttributeNameBefore(code)
  }

  /** @type {State} */
  function completeAttributeValueBefore(code) {
    if (
      code === codes.eof ||
      code === codes.lessThan ||
      code === codes.equalsTo ||
      code === codes.greaterThan ||
      code === codes.graveAccent
    ) {
      return nok(code)
    }

    if (code === codes.quotationMark || code === codes.apostrophe) {
      effects.consume(code)
      marker = code
      return completeAttributeValueQuoted
    }

    if (markdownSpace(code)) {
      effects.consume(code)
      return completeAttributeValueBefore
    }

    marker = null
    return completeAttributeValueUnquoted(code)
  }

  /** @type {State} */
  function completeAttributeValueQuoted(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      return nok(code)
    }

    if (code === marker) {
      effects.consume(code)
      return completeAttributeValueQuotedAfter
    }

    effects.consume(code)
    return completeAttributeValueQuoted
  }

  /** @type {State} */
  function completeAttributeValueUnquoted(code) {
    if (
      code === codes.eof ||
      code === codes.quotationMark ||
      code === codes.apostrophe ||
      code === codes.lessThan ||
      code === codes.equalsTo ||
      code === codes.greaterThan ||
      code === codes.graveAccent ||
      markdownLineEndingOrSpace(code)
    ) {
      return completeAttributeNameAfter(code)
    }

    effects.consume(code)
    return completeAttributeValueUnquoted
  }

  /** @type {State} */
  function completeAttributeValueQuotedAfter(code) {
    if (
      code === codes.slash ||
      code === codes.greaterThan ||
      markdownSpace(code)
    ) {
      return completeAttributeNameBefore(code)
    }

    return nok(code)
  }

  /** @type {State} */
  function completeEnd(code) {
    if (code === codes.greaterThan) {
      effects.consume(code)
      return completeAfter
    }

    return nok(code)
  }

  /** @type {State} */
  function completeAfter(code) {
    if (markdownSpace(code)) {
      effects.consume(code)
      return completeAfter
    }

    return code === codes.eof || markdownLineEnding(code)
      ? continuation(code)
      : nok(code)
  }

  /** @type {State} */
  function continuation(code) {
    if (code === codes.dash && kind === constants.htmlComment) {
      effects.consume(code)
      return continuationCommentInside
    }

    if (code === codes.lessThan && kind === constants.htmlRaw) {
      effects.consume(code)
      return continuationRawTagOpen
    }

    if (code === codes.greaterThan && kind === constants.htmlDeclaration) {
      effects.consume(code)
      return continuationClose
    }

    if (code === codes.questionMark && kind === constants.htmlInstruction) {
      effects.consume(code)
      return continuationDeclarationInside
    }

    if (code === codes.rightSquareBracket && kind === constants.htmlCdata) {
      effects.consume(code)
      return continuationCharacterDataInside
    }

    if (
      markdownLineEnding(code) &&
      (kind === constants.htmlBasic || kind === constants.htmlComplete)
    ) {
      return effects.check(
        nextBlankConstruct,
        continuationClose,
        continuationAtLineEnding
      )(code)
    }

    if (code === codes.eof || markdownLineEnding(code)) {
      return continuationAtLineEnding(code)
    }

    effects.consume(code)
    return continuation
  }

  /** @type {State} */
  function continuationAtLineEnding(code) {
    effects.exit(types.htmlFlowData)
    return htmlContinueStart(code)
  }

  /** @type {State} */
  function htmlContinueStart(code) {
    if (code === codes.eof) {
      return done(code)
    }

    if (markdownLineEnding(code)) {
      return effects.attempt(
        {tokenize: htmlLineEnd, partial: true},
        htmlContinueStart,
        done
      )(code)
    }

    effects.enter(types.htmlFlowData)
    return continuation(code)
  }

  /** @type {Tokenizer} */
  function htmlLineEnd(effects, ok, nok) {
    return start

    /** @type {State} */
    function start(code) {
      assert(markdownLineEnding(code), 'expected eol')
      effects.enter(types.lineEnding)
      effects.consume(code)
      effects.exit(types.lineEnding)
      return lineStart
    }

    /** @type {State} */
    function lineStart(code) {
      return self.parser.lazy[self.now().line] ? nok(code) : ok(code)
    }
  }

  /** @type {State} */
  function continuationCommentInside(code) {
    if (code === codes.dash) {
      effects.consume(code)
      return continuationDeclarationInside
    }

    return continuation(code)
  }

  /** @type {State} */
  function continuationRawTagOpen(code) {
    if (code === codes.slash) {
      effects.consume(code)
      buffer = ''
      return continuationRawEndTag
    }

    return continuation(code)
  }

  /** @type {State} */
  function continuationRawEndTag(code) {
    if (
      code === codes.greaterThan &&
      htmlRawNames.includes(buffer.toLowerCase())
    ) {
      effects.consume(code)
      return continuationClose
    }

    if (asciiAlpha(code) && buffer.length < constants.htmlRawSizeMax) {
      effects.consume(code)
      buffer += String.fromCharCode(code)
      return continuationRawEndTag
    }

    return continuation(code)
  }

  /** @type {State} */
  function continuationCharacterDataInside(code) {
    if (code === codes.rightSquareBracket) {
      effects.consume(code)
      return continuationDeclarationInside
    }

    return continuation(code)
  }

  /** @type {State} */
  function continuationDeclarationInside(code) {
    if (code === codes.greaterThan) {
      effects.consume(code)
      return continuationClose
    }

    return continuation(code)
  }

  /** @type {State} */
  function continuationClose(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      effects.exit(types.htmlFlowData)
      return done(code)
    }

    effects.consume(code)
    return continuationClose
  }

  /** @type {State} */
  function done(code) {
    effects.exit(types.htmlFlow)
    return ok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeNextBlank(effects, ok, nok) {
  return start

  /** @type {State} */
  function start(code) {
    assert(markdownLineEnding(code), 'expected a line ending')
    effects.exit(types.htmlFlowData)
    effects.enter(types.lineEndingBlank)
    effects.consume(code)
    effects.exit(types.lineEndingBlank)
    return effects.attempt(blankLine, ok, nok)
  }
}
