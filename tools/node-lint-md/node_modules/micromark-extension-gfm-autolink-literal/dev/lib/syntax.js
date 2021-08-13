/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').ConstructRecord} ConstructRecord
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Previous} Previous
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').Code} Code
 */

import assert from 'assert'
import {
  asciiAlpha,
  asciiAlphanumeric,
  asciiControl,
  asciiDigit,
  markdownLineEndingOrSpace,
  markdownLineEnding,
  unicodePunctuation,
  unicodeWhitespace
} from 'micromark-util-character'
import {codes} from 'micromark-util-symbol/codes.js'

const www = {tokenize: tokenizeWww, partial: true}
const domain = {tokenize: tokenizeDomain, partial: true}
const path = {tokenize: tokenizePath, partial: true}
const punctuation = {tokenize: tokenizePunctuation, partial: true}
const namedCharacterReference = {
  tokenize: tokenizeNamedCharacterReference,
  partial: true
}

const wwwAutolink = {tokenize: tokenizeWwwAutolink, previous: previousWww}
const httpAutolink = {tokenize: tokenizeHttpAutolink, previous: previousHttp}
const emailAutolink = {tokenize: tokenizeEmailAutolink, previous: previousEmail}

/** @type {ConstructRecord} */
const text = {}

/** @type {Extension} */
export const gfmAutolinkLiteral = {text}

let code = codes.digit0

// Add alphanumerics.
while (code < codes.leftCurlyBrace) {
  text[code] = emailAutolink
  code++
  if (code === codes.colon) code = codes.uppercaseA
  else if (code === codes.leftSquareBracket) code = codes.lowercaseA
}

text[codes.plusSign] = emailAutolink
text[codes.dash] = emailAutolink
text[codes.dot] = emailAutolink
text[codes.underscore] = emailAutolink
text[codes.uppercaseH] = [emailAutolink, httpAutolink]
text[codes.lowercaseH] = [emailAutolink, httpAutolink]
text[codes.uppercaseW] = [emailAutolink, wwwAutolink]
text[codes.lowercaseW] = [emailAutolink, wwwAutolink]

/** @type {Tokenizer} */
function tokenizeEmailAutolink(effects, ok, nok) {
  const self = this
  /** @type {boolean} */
  let hasDot
  /** @type {boolean|undefined} */
  let hasDigitInLastSegment

  return start

  /** @type {State} */
  function start(code) {
    if (
      !gfmAtext(code) ||
      !previousEmail(self.previous) ||
      previousUnbalanced(self.events)
    ) {
      return nok(code)
    }

    effects.enter('literalAutolink')
    effects.enter('literalAutolinkEmail')
    return atext(code)
  }

  /** @type {State} */
  function atext(code) {
    if (gfmAtext(code)) {
      effects.consume(code)
      return atext
    }

    if (code === codes.atSign) {
      effects.consume(code)
      return label
    }

    return nok(code)
  }

  /** @type {State} */
  function label(code) {
    if (code === codes.dot) {
      return effects.check(punctuation, done, dotContinuation)(code)
    }

    if (code === codes.dash || code === codes.underscore) {
      return effects.check(punctuation, nok, dashOrUnderscoreContinuation)(code)
    }

    if (asciiAlphanumeric(code)) {
      if (!hasDigitInLastSegment && asciiDigit(code)) {
        hasDigitInLastSegment = true
      }

      effects.consume(code)
      return label
    }

    return done(code)
  }

  /** @type {State} */
  function dotContinuation(code) {
    effects.consume(code)
    hasDot = true
    hasDigitInLastSegment = undefined
    return label
  }

  /** @type {State} */
  function dashOrUnderscoreContinuation(code) {
    effects.consume(code)
    return afterDashOrUnderscore
  }

  /** @type {State} */
  function afterDashOrUnderscore(code) {
    if (code === codes.dot) {
      return effects.check(punctuation, nok, dotContinuation)(code)
    }

    return label(code)
  }

  /** @type {State} */
  function done(code) {
    if (hasDot && !hasDigitInLastSegment) {
      effects.exit('literalAutolinkEmail')
      effects.exit('literalAutolink')
      return ok(code)
    }

    return nok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeWwwAutolink(effects, ok, nok) {
  const self = this

  return start

  /** @type {State} */
  function start(code) {
    if (
      (code !== codes.uppercaseW && code !== codes.lowercaseW) ||
      !previousWww(self.previous) ||
      previousUnbalanced(self.events)
    ) {
      return nok(code)
    }

    effects.enter('literalAutolink')
    effects.enter('literalAutolinkWww')
    // For `www.` we check instead of attempt, because when it matches, GH
    // treats it as part of a domain (yes, it says a valid domain must come
    // after `www.`, but that’s not how it’s implemented by them).
    return effects.check(
      www,
      effects.attempt(domain, effects.attempt(path, done), nok),
      nok
    )(code)
  }

  /** @type {State} */
  function done(code) {
    effects.exit('literalAutolinkWww')
    effects.exit('literalAutolink')
    return ok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeHttpAutolink(effects, ok, nok) {
  const self = this

  return start

  /** @type {State} */
  function start(code) {
    if (
      (code !== codes.uppercaseH && code !== codes.lowercaseH) ||
      !previousHttp(self.previous) ||
      previousUnbalanced(self.events)
    ) {
      return nok(code)
    }

    effects.enter('literalAutolink')
    effects.enter('literalAutolinkHttp')
    effects.consume(code)
    return t1
  }

  /** @type {State} */
  function t1(code) {
    if (code === codes.uppercaseT || code === codes.lowercaseT) {
      effects.consume(code)
      return t2
    }

    return nok(code)
  }

  /** @type {State} */
  function t2(code) {
    if (code === codes.uppercaseT || code === codes.lowercaseT) {
      effects.consume(code)
      return p
    }

    return nok(code)
  }

  /** @type {State} */
  function p(code) {
    if (code === codes.uppercaseP || code === codes.lowercaseP) {
      effects.consume(code)
      return s
    }

    return nok(code)
  }

  /** @type {State} */
  function s(code) {
    if (code === codes.uppercaseS || code === codes.lowercaseS) {
      effects.consume(code)
      return colon
    }

    return colon(code)
  }

  /** @type {State} */
  function colon(code) {
    if (code === codes.colon) {
      effects.consume(code)
      return slash1
    }

    return nok(code)
  }

  /** @type {State} */
  function slash1(code) {
    if (code === codes.slash) {
      effects.consume(code)
      return slash2
    }

    return nok(code)
  }

  /** @type {State} */
  function slash2(code) {
    if (code === codes.slash) {
      effects.consume(code)
      return after
    }

    return nok(code)
  }

  /** @type {State} */
  function after(code) {
    return code === codes.eof ||
      asciiControl(code) ||
      unicodeWhitespace(code) ||
      unicodePunctuation(code)
      ? nok(code)
      : effects.attempt(domain, effects.attempt(path, done), nok)(code)
  }

  /** @type {State} */
  function done(code) {
    effects.exit('literalAutolinkHttp')
    effects.exit('literalAutolink')
    return ok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeWww(effects, ok, nok) {
  return start

  /** @type {State} */
  function start(code) {
    assert(
      code === codes.uppercaseW || code === codes.lowercaseW,
      'expected `w`'
    )
    effects.consume(code)
    return w2
  }

  /** @type {State} */
  function w2(code) {
    if (code === codes.uppercaseW || code === codes.lowercaseW) {
      effects.consume(code)
      return w3
    }

    return nok(code)
  }

  /** @type {State} */
  function w3(code) {
    if (code === codes.uppercaseW || code === codes.lowercaseW) {
      effects.consume(code)
      return dot
    }

    return nok(code)
  }

  /** @type {State} */
  function dot(code) {
    if (code === codes.dot) {
      effects.consume(code)
      return after
    }

    return nok(code)
  }

  /** @type {State} */
  function after(code) {
    return code === codes.eof || markdownLineEnding(code) ? nok(code) : ok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeDomain(effects, ok, nok) {
  /** @type {boolean|undefined} */
  let hasUnderscoreInLastSegment
  /** @type {boolean|undefined} */
  let hasUnderscoreInLastLastSegment

  return domain

  /** @type {State} */
  function domain(code) {
    if (code === codes.ampersand) {
      return effects.check(
        namedCharacterReference,
        done,
        punctuationContinuation
      )(code)
    }

    if (code === codes.dot || code === codes.underscore) {
      return effects.check(punctuation, done, punctuationContinuation)(code)
    }

    // GH documents that only alphanumerics (other than `-`, `.`, and `_`) can
    // occur, which sounds like ASCII only, but they also support `www.點看.com`,
    // so that’s Unicode.
    // Instead of some new production for Unicode alphanumerics, markdown
    // already has that for Unicode punctuation and whitespace, so use those.
    if (
      code === codes.eof ||
      asciiControl(code) ||
      unicodeWhitespace(code) ||
      (code !== codes.dash && unicodePunctuation(code))
    ) {
      return done(code)
    }

    effects.consume(code)
    return domain
  }

  /** @type {State} */
  function punctuationContinuation(code) {
    if (code === codes.dot) {
      hasUnderscoreInLastLastSegment = hasUnderscoreInLastSegment
      hasUnderscoreInLastSegment = undefined
      effects.consume(code)
      return domain
    }

    if (code === codes.underscore) hasUnderscoreInLastSegment = true

    effects.consume(code)
    return domain
  }

  /** @type {State} */
  function done(code) {
    if (!hasUnderscoreInLastLastSegment && !hasUnderscoreInLastSegment) {
      return ok(code)
    }

    return nok(code)
  }
}

/** @type {Tokenizer} */
function tokenizePath(effects, ok) {
  let balance = 0

  return inPath

  /** @type {State} */
  function inPath(code) {
    if (code === codes.ampersand) {
      return effects.check(
        namedCharacterReference,
        ok,
        continuedPunctuation
      )(code)
    }

    if (code === codes.leftParenthesis) {
      balance++
    }

    if (code === codes.rightParenthesis) {
      return effects.check(
        punctuation,
        parenAtPathEnd,
        continuedPunctuation
      )(code)
    }

    if (pathEnd(code)) {
      return ok(code)
    }

    if (trailingPunctuation(code)) {
      return effects.check(punctuation, ok, continuedPunctuation)(code)
    }

    effects.consume(code)
    return inPath
  }

  /** @type {State} */
  function continuedPunctuation(code) {
    effects.consume(code)
    return inPath
  }

  /** @type {State} */
  function parenAtPathEnd(code) {
    balance--
    return balance < 0 ? ok(code) : continuedPunctuation(code)
  }
}

/** @type {Tokenizer} */
function tokenizeNamedCharacterReference(effects, ok, nok) {
  return start

  /** @type {State} */
  function start(code) {
    assert(code === codes.ampersand, 'expected `&`')
    effects.consume(code)
    return inside
  }

  /** @type {State} */
  function inside(code) {
    if (asciiAlpha(code)) {
      effects.consume(code)
      return inside
    }

    if (code === codes.semicolon) {
      effects.consume(code)
      return after
    }

    return nok(code)
  }

  /** @type {State} */
  function after(code) {
    // If the named character reference is followed by the end of the path, it’s
    // not continued punctuation.
    return pathEnd(code) ? ok(code) : nok(code)
  }
}

/** @type {Tokenizer} */
function tokenizePunctuation(effects, ok, nok) {
  return start

  /** @type {State} */
  function start(code) {
    assert(
      code === codes.dash || trailingPunctuation(code),
      'expected punctuation'
    )
    effects.consume(code)
    return after
  }

  /** @type {State} */
  function after(code) {
    // Check the next.
    if (trailingPunctuation(code)) {
      effects.consume(code)
      return after
    }

    // If the punctuation marker is followed by the end of the path, it’s not
    // continued punctuation.
    return pathEnd(code) ? ok(code) : nok(code)
  }
}

/**
 * @param {Code} code
 * @returns {boolean}
 */
function trailingPunctuation(code) {
  return (
    code === codes.exclamationMark ||
    code === codes.quotationMark ||
    code === codes.apostrophe ||
    code === codes.rightParenthesis ||
    code === codes.asterisk ||
    code === codes.comma ||
    code === codes.dot ||
    code === codes.colon ||
    code === codes.semicolon ||
    code === codes.lessThan ||
    code === codes.questionMark ||
    code === codes.underscore ||
    code === codes.tilde
  )
}

/**
 * @param {Code} code
 * @returns {boolean}
 */
function pathEnd(code) {
  return (
    code === codes.eof ||
    code === codes.lessThan ||
    markdownLineEndingOrSpace(code)
  )
}

/**
 * @param {Code} code
 * @returns {boolean}
 */
function gfmAtext(code) {
  return (
    code === codes.plusSign ||
    code === codes.dash ||
    code === codes.dot ||
    code === codes.underscore ||
    asciiAlphanumeric(code)
  )
}

/** @type {Previous} */
function previousWww(code) {
  return (
    code === codes.eof ||
    code === codes.leftParenthesis ||
    code === codes.asterisk ||
    code === codes.underscore ||
    code === codes.tilde ||
    markdownLineEndingOrSpace(code)
  )
}

/** @type {Previous} */
function previousHttp(code) {
  return code === codes.eof || !asciiAlpha(code)
}

/** @type {Previous} */
function previousEmail(code) {
  return code !== codes.slash && previousHttp(code)
}

/**
 * @param {Event[]} events
 * @returns {boolean}
 */
function previousUnbalanced(events) {
  let index = events.length
  let result = false

  while (index--) {
    const token = events[index][1]

    if (
      (token.type === 'labelLink' || token.type === 'labelImage') &&
      !token._balanced
    ) {
      result = true
      break
    }

    // @ts-expect-error If we’ve seen this token, and it was marked as not
    // having any unbalanced bracket before it, we can exit.
    if (token._gfmAutolinkLiteralWalkedInto) {
      result = false
      break
    }
  }

  if (events.length > 0 && !result) {
    // @ts-expect-error Mark the last token as “walked into” w/o finding
    // anything.
    events[events.length - 1][1]._gfmAutolinkLiteralWalkedInto = true
  }

  return result
}
