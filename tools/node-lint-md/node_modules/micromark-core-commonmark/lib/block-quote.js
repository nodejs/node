/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Exiter} Exiter
 * @typedef {import('micromark-util-types').State} State
 */
import {factorySpace} from 'micromark-factory-space'
import {markdownSpace} from 'micromark-util-character'

/** @type {Construct} */
export const blockQuote = {
  name: 'blockQuote',
  tokenize: tokenizeBlockQuoteStart,
  continuation: {
    tokenize: tokenizeBlockQuoteContinuation
  },
  exit
}
/** @type {Tokenizer} */

function tokenizeBlockQuoteStart(effects, ok, nok) {
  const self = this
  return start
  /** @type {State} */

  function start(code) {
    if (code === 62) {
      const state = self.containerState

      if (!state.open) {
        effects.enter('blockQuote', {
          _container: true
        })
        state.open = true
      }

      effects.enter('blockQuotePrefix')
      effects.enter('blockQuoteMarker')
      effects.consume(code)
      effects.exit('blockQuoteMarker')
      return after
    }

    return nok(code)
  }
  /** @type {State} */

  function after(code) {
    if (markdownSpace(code)) {
      effects.enter('blockQuotePrefixWhitespace')
      effects.consume(code)
      effects.exit('blockQuotePrefixWhitespace')
      effects.exit('blockQuotePrefix')
      return ok
    }

    effects.exit('blockQuotePrefix')
    return ok(code)
  }
}
/** @type {Tokenizer} */

function tokenizeBlockQuoteContinuation(effects, ok, nok) {
  return factorySpace(
    effects,
    effects.attempt(blockQuote, ok, nok),
    'linePrefix',
    this.parser.constructs.disable.null.includes('codeIndented') ? undefined : 4
  )
}
/** @type {Exiter} */

function exit(effects) {
  effects.exit('blockQuote')
}
