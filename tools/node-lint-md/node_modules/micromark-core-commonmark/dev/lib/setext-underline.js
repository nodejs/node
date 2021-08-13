/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

import assert from 'assert'
import {factorySpace} from 'micromark-factory-space'
import {markdownLineEnding} from 'micromark-util-character'
import {codes} from 'micromark-util-symbol/codes.js'
import {types} from 'micromark-util-symbol/types.js'

/** @type {Construct} */
export const setextUnderline = {
  name: 'setextUnderline',
  tokenize: tokenizeSetextUnderline,
  resolveTo: resolveToSetextUnderline
}

/** @type {Resolver} */
function resolveToSetextUnderline(events, context) {
  let index = events.length
  /** @type {number|undefined} */
  let content
  /** @type {number|undefined} */
  let text
  /** @type {number|undefined} */
  let definition

  // Find the opening of the content.
  // It’ll always exist: we don’t tokenize if it isn’t there.
  while (index--) {
    if (events[index][0] === 'enter') {
      if (events[index][1].type === types.content) {
        content = index
        break
      }

      if (events[index][1].type === types.paragraph) {
        text = index
      }
    }
    // Exit
    else {
      if (events[index][1].type === types.content) {
        // Remove the content end (if needed we’ll add it later)
        events.splice(index, 1)
      }

      if (!definition && events[index][1].type === types.definition) {
        definition = index
      }
    }
  }

  assert(text !== undefined, 'expected a `text` index to be found')
  assert(content !== undefined, 'expected a `text` index to be found')

  const heading = {
    type: types.setextHeading,
    start: Object.assign({}, events[text][1].start),
    end: Object.assign({}, events[events.length - 1][1].end)
  }

  // Change the paragraph to setext heading text.
  events[text][1].type = types.setextHeadingText

  // If we have definitions in the content, we’ll keep on having content,
  // but we need move it.
  if (definition) {
    events.splice(text, 0, ['enter', heading, context])
    events.splice(definition + 1, 0, ['exit', events[content][1], context])
    events[content][1].end = Object.assign({}, events[definition][1].end)
  } else {
    events[content][1] = heading
  }

  // Add the heading exit at the end.
  events.push(['exit', heading, context])

  return events
}

/** @type {Tokenizer} */
function tokenizeSetextUnderline(effects, ok, nok) {
  const self = this
  let index = self.events.length
  /** @type {NonNullable<Code>} */
  let marker
  /** @type {boolean} */
  let paragraph

  // Find an opening.
  while (index--) {
    // Skip enter/exit of line ending, line prefix, and content.
    // We can now either have a definition or a paragraph.
    if (
      self.events[index][1].type !== types.lineEnding &&
      self.events[index][1].type !== types.linePrefix &&
      self.events[index][1].type !== types.content
    ) {
      paragraph = self.events[index][1].type === types.paragraph
      break
    }
  }

  return start

  /** @type {State} */
  function start(code) {
    assert(
      code === codes.dash || code === codes.equalsTo,
      'expected `=` or `-`'
    )

    if (!self.parser.lazy[self.now().line] && (self.interrupt || paragraph)) {
      effects.enter(types.setextHeadingLine)
      effects.enter(types.setextHeadingLineSequence)
      marker = code
      return closingSequence(code)
    }

    return nok(code)
  }

  /** @type {State} */
  function closingSequence(code) {
    if (code === marker) {
      effects.consume(code)
      return closingSequence
    }

    effects.exit(types.setextHeadingLineSequence)
    return factorySpace(effects, closingSequenceEnd, types.lineSuffix)(code)
  }

  /** @type {State} */
  function closingSequenceEnd(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      effects.exit(types.setextHeadingLine)
      return ok(code)
    }

    return nok(code)
  }
}
