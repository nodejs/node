/**
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Code} Code
 */

import assert from 'assert'
import {factoryDestination} from 'micromark-factory-destination'
import {factoryLabel} from 'micromark-factory-label'
import {factoryTitle} from 'micromark-factory-title'
import {factoryWhitespace} from 'micromark-factory-whitespace'
import {markdownLineEndingOrSpace} from 'micromark-util-character'
import {push, splice} from 'micromark-util-chunked'
import {normalizeIdentifier} from 'micromark-util-normalize-identifier'
import {resolveAll} from 'micromark-util-resolve-all'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'

/** @type {Construct} */
export const labelEnd = {
  name: 'labelEnd',
  tokenize: tokenizeLabelEnd,
  resolveTo: resolveToLabelEnd,
  resolveAll: resolveAllLabelEnd
}

/** @type {Construct} */
const resourceConstruct = {tokenize: tokenizeResource}
/** @type {Construct} */
const fullReferenceConstruct = {tokenize: tokenizeFullReference}
/** @type {Construct} */
const collapsedReferenceConstruct = {tokenize: tokenizeCollapsedReference}

/** @type {Resolver} */
function resolveAllLabelEnd(events) {
  let index = -1
  /** @type {Token} */
  let token

  while (++index < events.length) {
    token = events[index][1]

    if (
      token.type === types.labelImage ||
      token.type === types.labelLink ||
      token.type === types.labelEnd
    ) {
      // Remove the marker.
      events.splice(index + 1, token.type === types.labelImage ? 4 : 2)
      token.type = types.data
      index++
    }
  }

  return events
}

/** @type {Resolver} */
function resolveToLabelEnd(events, context) {
  let index = events.length
  let offset = 0
  /** @type {Token} */
  let token
  /** @type {number|undefined} */
  let open
  /** @type {number|undefined} */
  let close
  /** @type {Event[]} */
  let media

  // Find an opening.
  while (index--) {
    token = events[index][1]

    if (open) {
      // If we see another link, or inactive link label, we’ve been here before.
      if (
        token.type === types.link ||
        (token.type === types.labelLink && token._inactive)
      ) {
        break
      }

      // Mark other link openings as inactive, as we can’t have links in
      // links.
      if (events[index][0] === 'enter' && token.type === types.labelLink) {
        token._inactive = true
      }
    } else if (close) {
      if (
        events[index][0] === 'enter' &&
        (token.type === types.labelImage || token.type === types.labelLink) &&
        !token._balanced
      ) {
        open = index

        if (token.type !== types.labelLink) {
          offset = 2
          break
        }
      }
    } else if (token.type === types.labelEnd) {
      close = index
    }
  }

  assert(open !== undefined, '`open` is supposed to be found')
  assert(close !== undefined, '`close` is supposed to be found')

  const group = {
    type: events[open][1].type === types.labelLink ? types.link : types.image,
    start: Object.assign({}, events[open][1].start),
    end: Object.assign({}, events[events.length - 1][1].end)
  }

  const label = {
    type: types.label,
    start: Object.assign({}, events[open][1].start),
    end: Object.assign({}, events[close][1].end)
  }

  const text = {
    type: types.labelText,
    start: Object.assign({}, events[open + offset + 2][1].end),
    end: Object.assign({}, events[close - 2][1].start)
  }

  media = [
    ['enter', group, context],
    ['enter', label, context]
  ]

  // Opening marker.
  media = push(media, events.slice(open + 1, open + offset + 3))

  // Text open.
  media = push(media, [['enter', text, context]])

  // Between.
  media = push(
    media,
    resolveAll(
      context.parser.constructs.insideSpan.null,
      events.slice(open + offset + 4, close - 3),
      context
    )
  )

  // Text close, marker close, label close.
  media = push(media, [
    ['exit', text, context],
    events[close - 2],
    events[close - 1],
    ['exit', label, context]
  ])

  // Reference, resource, or so.
  media = push(media, events.slice(close + 1))

  // Media close.
  media = push(media, [['exit', group, context]])

  splice(events, open, events.length, media)

  return events
}

/** @type {Tokenizer} */
function tokenizeLabelEnd(effects, ok, nok) {
  const self = this
  let index = self.events.length
  /** @type {Token} */
  let labelStart
  /** @type {boolean} */
  let defined

  // Find an opening.
  while (index--) {
    if (
      (self.events[index][1].type === types.labelImage ||
        self.events[index][1].type === types.labelLink) &&
      !self.events[index][1]._balanced
    ) {
      labelStart = self.events[index][1]
      break
    }
  }

  return start

  /** @type {State} */
  function start(code) {
    assert(code === codes.rightSquareBracket, 'expected `]`')

    if (!labelStart) {
      return nok(code)
    }

    // It’s a balanced bracket, but contains a link.
    if (labelStart._inactive) return balanced(code)
    defined = self.parser.defined.includes(
      normalizeIdentifier(
        self.sliceSerialize({start: labelStart.end, end: self.now()})
      )
    )
    effects.enter(types.labelEnd)
    effects.enter(types.labelMarker)
    effects.consume(code)
    effects.exit(types.labelMarker)
    effects.exit(types.labelEnd)
    return afterLabelEnd
  }

  /** @type {State} */
  function afterLabelEnd(code) {
    // Resource: `[asd](fgh)`.
    if (code === codes.leftParenthesis) {
      return effects.attempt(
        resourceConstruct,
        ok,
        defined ? ok : balanced
      )(code)
    }

    // Collapsed (`[asd][]`) or full (`[asd][fgh]`) reference?
    if (code === codes.leftSquareBracket) {
      return effects.attempt(
        fullReferenceConstruct,
        ok,
        defined
          ? effects.attempt(collapsedReferenceConstruct, ok, balanced)
          : balanced
      )(code)
    }

    // Shortcut reference: `[asd]`?
    return defined ? ok(code) : balanced(code)
  }

  /** @type {State} */
  function balanced(code) {
    labelStart._balanced = true
    return nok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeResource(effects, ok, nok) {
  return start

  /** @type {State} */
  function start(code) {
    assert.strictEqual(code, codes.leftParenthesis, 'expected left paren')
    effects.enter(types.resource)
    effects.enter(types.resourceMarker)
    effects.consume(code)
    effects.exit(types.resourceMarker)
    return factoryWhitespace(effects, open)
  }

  /** @type {State} */
  function open(code) {
    if (code === codes.rightParenthesis) {
      return end(code)
    }

    return factoryDestination(
      effects,
      destinationAfter,
      nok,
      types.resourceDestination,
      types.resourceDestinationLiteral,
      types.resourceDestinationLiteralMarker,
      types.resourceDestinationRaw,
      types.resourceDestinationString,
      constants.linkResourceDestinationBalanceMax
    )(code)
  }

  /** @type {State} */
  function destinationAfter(code) {
    return markdownLineEndingOrSpace(code)
      ? factoryWhitespace(effects, between)(code)
      : end(code)
  }

  /** @type {State} */
  function between(code) {
    if (
      code === codes.quotationMark ||
      code === codes.apostrophe ||
      code === codes.leftParenthesis
    ) {
      return factoryTitle(
        effects,
        factoryWhitespace(effects, end),
        nok,
        types.resourceTitle,
        types.resourceTitleMarker,
        types.resourceTitleString
      )(code)
    }

    return end(code)
  }

  /** @type {State} */
  function end(code) {
    if (code === codes.rightParenthesis) {
      effects.enter(types.resourceMarker)
      effects.consume(code)
      effects.exit(types.resourceMarker)
      effects.exit(types.resource)
      return ok
    }

    return nok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeFullReference(effects, ok, nok) {
  const self = this

  return start

  /** @type {State} */
  function start(code) {
    assert.strictEqual(code, codes.leftSquareBracket, 'expected left bracket')
    return factoryLabel.call(
      self,
      effects,
      afterLabel,
      nok,
      types.reference,
      types.referenceMarker,
      types.referenceString
    )(code)
  }

  /** @type {State} */
  function afterLabel(code) {
    return self.parser.defined.includes(
      normalizeIdentifier(
        self.sliceSerialize(self.events[self.events.length - 1][1]).slice(1, -1)
      )
    )
      ? ok(code)
      : nok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeCollapsedReference(effects, ok, nok) {
  return start

  /** @type {State} */
  function start(code) {
    assert.strictEqual(code, codes.leftSquareBracket, 'expected left bracket')
    effects.enter(types.reference)
    effects.enter(types.referenceMarker)
    effects.consume(code)
    effects.exit(types.referenceMarker)
    return open
  }

  /** @type {State} */
  function open(code) {
    if (code === codes.rightSquareBracket) {
      effects.enter(types.referenceMarker)
      effects.consume(code)
      effects.exit(types.referenceMarker)
      effects.exit(types.reference)
      return ok
    }

    return nok(code)
  }
}
