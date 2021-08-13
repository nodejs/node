/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 */

import assert from 'assert'
import {factoryDestination} from 'micromark-factory-destination'
import {factoryLabel} from 'micromark-factory-label'
import {factorySpace} from 'micromark-factory-space'
import {factoryTitle} from 'micromark-factory-title'
import {factoryWhitespace} from 'micromark-factory-whitespace'
import {normalizeIdentifier} from 'micromark-util-normalize-identifier'
import {
  markdownLineEnding,
  markdownLineEndingOrSpace
} from 'micromark-util-character'
import {codes} from 'micromark-util-symbol/codes.js'
import {types} from 'micromark-util-symbol/types.js'

/** @type {Construct} */
export const definition = {name: 'definition', tokenize: tokenizeDefinition}

/** @type {Construct} */
const titleConstruct = {tokenize: tokenizeTitle, partial: true}

/** @type {Tokenizer} */
function tokenizeDefinition(effects, ok, nok) {
  const self = this
  /** @type {string} */
  let identifier

  return start

  /** @type {State} */
  function start(code) {
    assert(code === codes.leftSquareBracket, 'expected `[`')
    effects.enter(types.definition)
    return factoryLabel.call(
      self,
      effects,
      labelAfter,
      nok,
      types.definitionLabel,
      types.definitionLabelMarker,
      types.definitionLabelString
    )(code)
  }

  /** @type {State} */
  function labelAfter(code) {
    identifier = normalizeIdentifier(
      self.sliceSerialize(self.events[self.events.length - 1][1]).slice(1, -1)
    )

    if (code === codes.colon) {
      effects.enter(types.definitionMarker)
      effects.consume(code)
      effects.exit(types.definitionMarker)

      // Note: blank lines can’t exist in content.
      return factoryWhitespace(
        effects,
        factoryDestination(
          effects,
          effects.attempt(
            titleConstruct,
            factorySpace(effects, after, types.whitespace),
            factorySpace(effects, after, types.whitespace)
          ),
          nok,
          types.definitionDestination,
          types.definitionDestinationLiteral,
          types.definitionDestinationLiteralMarker,
          types.definitionDestinationRaw,
          types.definitionDestinationString
        )
      )
    }

    return nok(code)
  }

  /** @type {State} */
  function after(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      effects.exit(types.definition)

      if (!self.parser.defined.includes(identifier)) {
        self.parser.defined.push(identifier)
      }

      return ok(code)
    }

    return nok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeTitle(effects, ok, nok) {
  return start

  /** @type {State} */
  function start(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, before)(code)
      : nok(code)
  }

  /** @type {State} */
  function before(code) {
    if (
      code === codes.quotationMark ||
      code === codes.apostrophe ||
      code === codes.leftParenthesis
    ) {
      return factoryTitle(
        effects,
        factorySpace(effects, after, types.whitespace),
        nok,
        types.definitionTitle,
        types.definitionTitleMarker,
        types.definitionTitleString
      )(code)
    }

    return nok(code)
  }

  /** @type {State} */
  function after(code) {
    return code === codes.eof || markdownLineEnding(code) ? ok(code) : nok(code)
  }
}
