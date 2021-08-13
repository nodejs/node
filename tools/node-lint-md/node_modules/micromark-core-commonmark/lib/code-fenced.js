/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */
import {factorySpace} from 'micromark-factory-space'
import {
  markdownLineEnding,
  markdownLineEndingOrSpace
} from 'micromark-util-character'

/** @type {Construct} */
export const codeFenced = {
  name: 'codeFenced',
  tokenize: tokenizeCodeFenced,
  concrete: true
}
/** @type {Tokenizer} */

function tokenizeCodeFenced(effects, ok, nok) {
  const self = this
  /** @type {Construct} */

  const closingFenceConstruct = {
    tokenize: tokenizeClosingFence,
    partial: true
  }
  /** @type {Construct} */

  const nonLazyLine = {
    tokenize: tokenizeNonLazyLine,
    partial: true
  }
  const tail = this.events[this.events.length - 1]
  const initialPrefix =
    tail && tail[1].type === 'linePrefix'
      ? tail[2].sliceSerialize(tail[1], true).length
      : 0
  let sizeOpen = 0
  /** @type {NonNullable<Code>} */

  let marker
  return start
  /** @type {State} */

  function start(code) {
    effects.enter('codeFenced')
    effects.enter('codeFencedFence')
    effects.enter('codeFencedFenceSequence')
    marker = code
    return sequenceOpen(code)
  }
  /** @type {State} */

  function sequenceOpen(code) {
    if (code === marker) {
      effects.consume(code)
      sizeOpen++
      return sequenceOpen
    }

    effects.exit('codeFencedFenceSequence')
    return sizeOpen < 3
      ? nok(code)
      : factorySpace(effects, infoOpen, 'whitespace')(code)
  }
  /** @type {State} */

  function infoOpen(code) {
    if (code === null || markdownLineEnding(code)) {
      return openAfter(code)
    }

    effects.enter('codeFencedFenceInfo')
    effects.enter('chunkString', {
      contentType: 'string'
    })
    return info(code)
  }
  /** @type {State} */

  function info(code) {
    if (code === null || markdownLineEndingOrSpace(code)) {
      effects.exit('chunkString')
      effects.exit('codeFencedFenceInfo')
      return factorySpace(effects, infoAfter, 'whitespace')(code)
    }

    if (code === 96 && code === marker) return nok(code)
    effects.consume(code)
    return info
  }
  /** @type {State} */

  function infoAfter(code) {
    if (code === null || markdownLineEnding(code)) {
      return openAfter(code)
    }

    effects.enter('codeFencedFenceMeta')
    effects.enter('chunkString', {
      contentType: 'string'
    })
    return meta(code)
  }
  /** @type {State} */

  function meta(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('chunkString')
      effects.exit('codeFencedFenceMeta')
      return openAfter(code)
    }

    if (code === 96 && code === marker) return nok(code)
    effects.consume(code)
    return meta
  }
  /** @type {State} */

  function openAfter(code) {
    effects.exit('codeFencedFence')
    return self.interrupt ? ok(code) : contentStart(code)
  }
  /** @type {State} */

  function contentStart(code) {
    if (code === null) {
      return after(code)
    }

    if (markdownLineEnding(code)) {
      return effects.attempt(
        nonLazyLine,
        effects.attempt(
          closingFenceConstruct,
          after,
          initialPrefix
            ? factorySpace(
                effects,
                contentStart,
                'linePrefix',
                initialPrefix + 1
              )
            : contentStart
        ),
        after
      )(code)
    }

    effects.enter('codeFlowValue')
    return contentContinue(code)
  }
  /** @type {State} */

  function contentContinue(code) {
    if (code === null || markdownLineEnding(code)) {
      effects.exit('codeFlowValue')
      return contentStart(code)
    }

    effects.consume(code)
    return contentContinue
  }
  /** @type {State} */

  function after(code) {
    effects.exit('codeFenced')
    return ok(code)
  }
  /** @type {Tokenizer} */

  function tokenizeNonLazyLine(effects, ok, nok) {
    const self = this
    return start
    /** @type {State} */

    function start(code) {
      effects.enter('lineEnding')
      effects.consume(code)
      effects.exit('lineEnding')
      return lineStart
    }
    /** @type {State} */

    function lineStart(code) {
      return self.parser.lazy[self.now().line] ? nok(code) : ok(code)
    }
  }
  /** @type {Tokenizer} */

  function tokenizeClosingFence(effects, ok, nok) {
    let size = 0
    return factorySpace(
      effects,
      closingSequenceStart,
      'linePrefix',
      this.parser.constructs.disable.null.includes('codeIndented')
        ? undefined
        : 4
    )
    /** @type {State} */

    function closingSequenceStart(code) {
      effects.enter('codeFencedFence')
      effects.enter('codeFencedFenceSequence')
      return closingSequence(code)
    }
    /** @type {State} */

    function closingSequence(code) {
      if (code === marker) {
        effects.consume(code)
        size++
        return closingSequence
      }

      if (size < sizeOpen) return nok(code)
      effects.exit('codeFencedFenceSequence')
      return factorySpace(effects, closingSequenceEnd, 'whitespace')(code)
    }
    /** @type {State} */

    function closingSequenceEnd(code) {
      if (code === null || markdownLineEnding(code)) {
        effects.exit('codeFencedFence')
        return ok(code)
      }

      return nok(code)
    }
  }
}
