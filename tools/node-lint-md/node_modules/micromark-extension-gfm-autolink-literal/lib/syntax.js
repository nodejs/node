/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').ConstructRecord} ConstructRecord
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').Previous} Previous
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').Code} Code
 */
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
const www = {
  tokenize: tokenizeWww,
  partial: true
}
const domain = {
  tokenize: tokenizeDomain,
  partial: true
}
const path = {
  tokenize: tokenizePath,
  partial: true
}
const punctuation = {
  tokenize: tokenizePunctuation,
  partial: true
}
const namedCharacterReference = {
  tokenize: tokenizeNamedCharacterReference,
  partial: true
}
const wwwAutolink = {
  tokenize: tokenizeWwwAutolink,
  previous: previousWww
}
const httpAutolink = {
  tokenize: tokenizeHttpAutolink,
  previous: previousHttp
}
const emailAutolink = {
  tokenize: tokenizeEmailAutolink,
  previous: previousEmail
}
/** @type {ConstructRecord} */

const text = {}
/** @type {Extension} */

export const gfmAutolinkLiteral = {
  text
}
let code = 48 // Add alphanumerics.

while (code < 123) {
  text[code] = emailAutolink
  code++
  if (code === 58) code = 65
  else if (code === 91) code = 97
}

text[43] = emailAutolink
text[45] = emailAutolink
text[46] = emailAutolink
text[95] = emailAutolink
text[72] = [emailAutolink, httpAutolink]
text[104] = [emailAutolink, httpAutolink]
text[87] = [emailAutolink, wwwAutolink]
text[119] = [emailAutolink, wwwAutolink]
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

    if (code === 64) {
      effects.consume(code)
      return label
    }

    return nok(code)
  }
  /** @type {State} */

  function label(code) {
    if (code === 46) {
      return effects.check(punctuation, done, dotContinuation)(code)
    }

    if (code === 45 || code === 95) {
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
    if (code === 46) {
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
      (code !== 87 && code !== 119) ||
      !previousWww(self.previous) ||
      previousUnbalanced(self.events)
    ) {
      return nok(code)
    }

    effects.enter('literalAutolink')
    effects.enter('literalAutolinkWww') // For `www.` we check instead of attempt, because when it matches, GH
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
      (code !== 72 && code !== 104) ||
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
    if (code === 84 || code === 116) {
      effects.consume(code)
      return t2
    }

    return nok(code)
  }
  /** @type {State} */

  function t2(code) {
    if (code === 84 || code === 116) {
      effects.consume(code)
      return p
    }

    return nok(code)
  }
  /** @type {State} */

  function p(code) {
    if (code === 80 || code === 112) {
      effects.consume(code)
      return s
    }

    return nok(code)
  }
  /** @type {State} */

  function s(code) {
    if (code === 83 || code === 115) {
      effects.consume(code)
      return colon
    }

    return colon(code)
  }
  /** @type {State} */

  function colon(code) {
    if (code === 58) {
      effects.consume(code)
      return slash1
    }

    return nok(code)
  }
  /** @type {State} */

  function slash1(code) {
    if (code === 47) {
      effects.consume(code)
      return slash2
    }

    return nok(code)
  }
  /** @type {State} */

  function slash2(code) {
    if (code === 47) {
      effects.consume(code)
      return after
    }

    return nok(code)
  }
  /** @type {State} */

  function after(code) {
    return code === null ||
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
    effects.consume(code)
    return w2
  }
  /** @type {State} */

  function w2(code) {
    if (code === 87 || code === 119) {
      effects.consume(code)
      return w3
    }

    return nok(code)
  }
  /** @type {State} */

  function w3(code) {
    if (code === 87 || code === 119) {
      effects.consume(code)
      return dot
    }

    return nok(code)
  }
  /** @type {State} */

  function dot(code) {
    if (code === 46) {
      effects.consume(code)
      return after
    }

    return nok(code)
  }
  /** @type {State} */

  function after(code) {
    return code === null || markdownLineEnding(code) ? nok(code) : ok(code)
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
    if (code === 38) {
      return effects.check(
        namedCharacterReference,
        done,
        punctuationContinuation
      )(code)
    }

    if (code === 46 || code === 95) {
      return effects.check(punctuation, done, punctuationContinuation)(code)
    } // GH documents that only alphanumerics (other than `-`, `.`, and `_`) can
    // occur, which sounds like ASCII only, but they also support `www.點看.com`,
    // so that’s Unicode.
    // Instead of some new production for Unicode alphanumerics, markdown
    // already has that for Unicode punctuation and whitespace, so use those.

    if (
      code === null ||
      asciiControl(code) ||
      unicodeWhitespace(code) ||
      (code !== 45 && unicodePunctuation(code))
    ) {
      return done(code)
    }

    effects.consume(code)
    return domain
  }
  /** @type {State} */

  function punctuationContinuation(code) {
    if (code === 46) {
      hasUnderscoreInLastLastSegment = hasUnderscoreInLastSegment
      hasUnderscoreInLastSegment = undefined
      effects.consume(code)
      return domain
    }

    if (code === 95) hasUnderscoreInLastSegment = true
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
    if (code === 38) {
      return effects.check(
        namedCharacterReference,
        ok,
        continuedPunctuation
      )(code)
    }

    if (code === 40) {
      balance++
    }

    if (code === 41) {
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
    effects.consume(code)
    return inside
  }
  /** @type {State} */

  function inside(code) {
    if (asciiAlpha(code)) {
      effects.consume(code)
      return inside
    }

    if (code === 59) {
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
    effects.consume(code)
    return after
  }
  /** @type {State} */

  function after(code) {
    // Check the next.
    if (trailingPunctuation(code)) {
      effects.consume(code)
      return after
    } // If the punctuation marker is followed by the end of the path, it’s not
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
    code === 33 ||
    code === 34 ||
    code === 39 ||
    code === 41 ||
    code === 42 ||
    code === 44 ||
    code === 46 ||
    code === 58 ||
    code === 59 ||
    code === 60 ||
    code === 63 ||
    code === 95 ||
    code === 126
  )
}
/**
 * @param {Code} code
 * @returns {boolean}
 */

function pathEnd(code) {
  return code === null || code === 60 || markdownLineEndingOrSpace(code)
}
/**
 * @param {Code} code
 * @returns {boolean}
 */

function gfmAtext(code) {
  return (
    code === 43 ||
    code === 45 ||
    code === 46 ||
    code === 95 ||
    asciiAlphanumeric(code)
  )
}
/** @type {Previous} */

function previousWww(code) {
  return (
    code === null ||
    code === 40 ||
    code === 42 ||
    code === 95 ||
    code === 126 ||
    markdownLineEndingOrSpace(code)
  )
}
/** @type {Previous} */

function previousHttp(code) {
  return code === null || !asciiAlpha(code)
}
/** @type {Previous} */

function previousEmail(code) {
  return code !== 47 && previousHttp(code)
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
    } // @ts-expect-error If we’ve seen this token, and it was marked as not
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
