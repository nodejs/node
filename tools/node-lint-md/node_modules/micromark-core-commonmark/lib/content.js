/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 */
import {factorySpace} from 'micromark-factory-space'
import {markdownLineEnding} from 'micromark-util-character'
import {subtokenize} from 'micromark-util-subtokenize'

/**
 * No name because it must not be turned off.
 * @type {Construct}
 */
export const content = {
  tokenize: tokenizeContent,
  resolve: resolveContent
}
/** @type {Construct} */

const continuationConstruct = {
  tokenize: tokenizeContinuation,
  partial: true
}
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
    effects.enter('content')
    previous = effects.enter('chunkContent', {
      contentType: 'content'
    })
    return data(code)
  }
  /** @type {State} */

  function data(code) {
    if (code === null) {
      return contentEnd(code)
    }

    if (markdownLineEnding(code)) {
      return effects.check(
        continuationConstruct,
        contentContinue,
        contentEnd
      )(code)
    } // Data.

    effects.consume(code)
    return data
  }
  /** @type {State} */

  function contentEnd(code) {
    effects.exit('chunkContent')
    effects.exit('content')
    return ok(code)
  }
  /** @type {State} */

  function contentContinue(code) {
    effects.consume(code)
    effects.exit('chunkContent')
    previous.next = effects.enter('chunkContent', {
      contentType: 'content',
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
    effects.exit('chunkContent')
    effects.enter('lineEnding')
    effects.consume(code)
    effects.exit('lineEnding')
    return factorySpace(effects, prefixed, 'linePrefix')
  }
  /** @type {State} */

  function prefixed(code) {
    if (code === null || markdownLineEnding(code)) {
      return nok(code)
    }

    const tail = self.events[self.events.length - 1]

    if (
      !self.parser.constructs.disable.null.includes('codeIndented') &&
      tail &&
      tail[1].type === 'linePrefix' &&
      tail[2].sliceSerialize(tail[1], true).length >= 4
    ) {
      return ok(code)
    }

    return effects.interrupt(self.parser.constructs.flow, nok, ok)(code)
  }
}
