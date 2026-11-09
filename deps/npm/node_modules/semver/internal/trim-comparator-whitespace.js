// @ts-check
'use strict'

// Reproduce safeRe[COMPARATORTRIM] replacement boundaries, not version
// validation. Each end-index finder returns an exclusive end, or its start
// on no match.
// Positions count from zero. An end of 3 means characters 0, 1, and 2 were read;
// range.slice(start, end) gives the matched text, not a converted number.
const {
  MAX_LENGTH,
  MAX_SAFE_BUILD_LENGTH,
  WHITESPACE_CHARACTERS,
} = require('./constants')

/**
 * Remembers the last two rows of digits so their characters need not be reread.
 * Positions start at -1 until a row has been found.
 *
 * @typedef {object} DigitRunCache
 * @property {number} start The first character of the most recently found row.
 * @property {number} end The position after that row's last character.
 * @property {number} previousStart The first character of the previous row.
 * @property {number} previousEnd The position after the previous row.
 */

// One entry per UTF-16 code unit keeps membership checks constant-time.
const whitespace = new Uint8Array(0x10000)
for (const char of WHITESPACE_CHARACTERS) {
  whitespace[char.charCodeAt(0)] = 1
}

/**
 * Checks whether a character is whitespace, like a space, tab, or newline.
 * Looking past the end of the string returns false.
 *
 * @param {string|undefined} char One character, or undefined past the string's end.
 * @returns {boolean} Whether the character is whitespace.
 * @example isWhitespace(' ') // true
 * @example isWhitespace('1') // false
 */
const isWhitespace = char =>
  char !== undefined && whitespace[char.charCodeAt(0)] === 1

/**
 * Checks for v, =, or whitespace before the version number.
 * These are historical version prefixes, not range operators:
 * < and > are handled separately, as are tilde and caret ranges.
 *
 * @param {string|undefined} char One character to check.
 * @returns {boolean} Whether the character can belong to a version prefix.
 * @example isVersionPrefix('=') // true
 * @example isVersionPrefix('>') // false
 */
const isVersionPrefix = char =>
  char === 'v' ||
  char === '=' ||
  isWhitespace(char)

/**
 * Checks whether a character is one of the digits 0 through 9.
 *
 * @param {string|undefined} char One character to check.
 * @returns {boolean} Whether the character is an ASCII digit.
 * @example isDigit('7') // true
 * @example isDigit('.') // false
 */
const isDigit = char => char !== undefined && char >= '0' && char <= '9'

/**
 * Checks whether a character is an English letter, uppercase or lowercase.
 *
 * @param {string|undefined} char One character to check.
 * @returns {boolean} Whether the character is an ASCII letter.
 * @example isLetter('b') // true
 * @example isLetter('7') // false
 */
const isLetter = char =>
  char !== undefined && (
    (char >= 'a' && char <= 'z') ||
    (char >= 'A' && char <= 'Z')
  )

/**
 * Checks for a letter, digit, or hyphen: the characters allowed inside
 * prerelease and build names such as beta-1.
 *
 * @param {string|undefined} char One character to check.
 * @returns {boolean} Whether the character can be part of an identifier.
 * @example isIdentifierCharacter('-') // true
 * @example isIdentifierCharacter('.') // false; dots separate identifiers
 */
const isIdentifierCharacter = char => isDigit(char) || isLetter(char) || char === '-'

/**
 * Checks whether a version component can begin here: with a digit or
 * a wildcard such as x, X, or *.
 *
 * @param {string|undefined} char One character to check.
 * @returns {boolean} Whether a number or wildcard can start with this character.
 * @example isVersionStart('x') // true
 * @example isVersionStart('v') // false; v belongs to the prefix
 */
const isVersionStart = char =>
  isDigit(char) ||
  char === 'x' ||
  char === 'X' ||
  char === '*'

/**
 * Finds the end of a row of digits, but returns no more than limit digits.
 * Remembers the last two rows so looking at them again does not reread them.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position of the first character to check.
 * @param {number} limit The most digit characters the returned match can include.
 * @param {DigitRunCache} digitRuns Shared cache of digit-run positions, updated in place.
 * @returns {number} The position after the matched digits, or start if none match.
 * @example findDigitsEndIndex('123abc', 0, 10, digitRuns) // 3, pointing at a
 * @example findDigitsEndIndex('123abc', 0, 2, digitRuns) // 2, pointing at the unread 3
 */
const findDigitsEndIndex = (range, start, limit, digitRuns) => {
  if (!isDigit(range[start])) {
    return start
  }
  if (start >= digitRuns.start && start < digitRuns.end) {
    return Math.min(start + limit, digitRuns.end)
  }
  if (start >= digitRuns.previousStart && start < digitRuns.previousEnd) {
    return Math.min(start + limit, digitRuns.previousEnd)
  }
  let end = start + 1
  while (isDigit(range[end])) {
    end++
  }
  // A failed full version can rewind from its second run to its first.
  digitRuns.previousStart = digitRuns.start
  digitRuns.previousEnd = digitRuns.end
  digitRuns.start = start
  digitRuns.end = end
  return Math.min(start + limit, end)
}

/**
 * Reads a number using the old regex's length and leading-zero rules.
 * Strict mode reads just the first zero in 012; loose mode can read all three.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position where the number should begin.
 * @param {boolean} loose Whether to allow leading zeros.
 * @param {DigitRunCache} digitRuns Shared digit-run cache, updated as needed.
 * @returns {number} The position after the matched digits, not their numeric value.
 * @example findVersionNumberEndIndex('012', 0, false, digitRuns) // 1
 * @example findVersionNumberEndIndex('012', 0, true, digitRuns) // 3
 */
const findVersionNumberEndIndex = (range, start, loose, digitRuns) => {
  if (!loose && range[start] === '0') {
    return start + 1
  }
  // Strict syntax allows an initial nonzero digit plus MAX_LENGTH digits.
  return findDigitsEndIndex(range, start, loose ? MAX_LENGTH : MAX_LENGTH + 1, digitRuns)
}

/**
 * Reads one piece of a prerelease name, such as beta or 1 in beta.1.
 * Tries the letter-or-hyphen form first, then the number-only form.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position where the identifier should begin.
 * @param {boolean} loose Whether numeric identifiers may have leading zeros.
 * @param {DigitRunCache} digitRuns Shared digit-run cache, updated as needed.
 * @returns {number} The position after the identifier, or start if none matches.
 * @example findPrereleaseIdentifierEndIndex('beta.1', 0, false, digitRuns) // 4, at the dot
 * @example findPrereleaseIdentifierEndIndex('001beta', 0, false, digitRuns) // 7
 */
const findPrereleaseIdentifierEndIndex = (range, start, loose, digitRuns) => {
  const digitsEnd = findDigitsEndIndex(range, start, MAX_LENGTH, digitRuns)
  // The legacy regex tries a nonnumeric identifier before a numeric one.
  if (isLetter(range[digitsEnd]) || range[digitsEnd] === '-') {
    let end = digitsEnd + 1
    const limit = end + MAX_SAFE_BUILD_LENGTH
    while (end < limit && isIdentifierCharacter(range[end])) {
      end++
    }
    return end
  }
  return findVersionNumberEndIndex(range, start, loose, digitRuns)
}

/**
 * Reads a prerelease suffix such as -beta.1, one dot-separated piece at a time.
 * Loose mode also allows the name without its opening hyphen.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position where the suffix should begin, including -.
 * @param {boolean} loose Whether the opening hyphen is optional and zeros allowed.
 * @param {DigitRunCache} digitRuns Shared digit-run cache, updated as needed.
 * @returns {number} The position after the matched suffix, or start if absent.
 * @example findPrereleaseEndIndex('-beta.1+sha', 0, false, digitRuns) // 7, at the plus
 * @example findPrereleaseEndIndex('beta', 0, false, digitRuns) // 0; no opening hyphen
 */
const findPrereleaseEndIndex = (range, start, loose, digitRuns) => {
  const hyphen = range[start] === '-'
  if (!loose && !hyphen) {
    return start
  }
  const identifierStart = start + (hyphen ? 1 : 0)
  let end = findPrereleaseIdentifierEndIndex(range, identifierStart, loose, digitRuns)
  if (end === identifierStart) {
    if (!loose || !hyphen) {
      return start
    }
    // In loose syntax, a lone hyphen can itself be the identifier.
    end = findPrereleaseIdentifierEndIndex(range, start, loose, digitRuns)
  }
  while (range[end] === '.') {
    const next = findPrereleaseIdentifierEndIndex(range, end + 1, loose, digitRuns)
    if (next === end + 1) {
      break
    }
    end = next
  }
  return end
}

/**
 * Reads one build-name piece, such as sha or 123 in +sha.123.
 * Stops at a disallowed character or the old regex's length limit.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position where the identifier should begin.
 * @returns {number} The position after the identifier, or start if none matches.
 * @example findBuildIdentifierEndIndex('sha.123', 0) // 3, at the dot
 * @example findBuildIdentifierEndIndex('001', 0) // 3; leading zeros are allowed
 */
const findBuildIdentifierEndIndex = (range, start) => {
  let end = start
  while (end - start < MAX_SAFE_BUILD_LENGTH && isIdentifierCharacter(range[end])) {
    end++
  }
  return end
}

/**
 * Reads optional build metadata such as +sha.123.
 * Leaves the position unchanged if there is no plus sign followed by a name.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position where the opening + should be.
 * @returns {number} The position after the matched metadata, or start if absent.
 * @example findBuildEndIndex('+sha.123 <2', 0) // 8, at the space
 * @example findBuildEndIndex('+', 0) // 0; a plus sign alone is not a match
 */
const findBuildEndIndex = (range, start) => {
  if (range[start] !== '+') {
    return start
  }
  let end = findBuildIdentifierEndIndex(range, start + 1)
  if (end === start + 1) {
    return start
  }
  while (range[end] === '.') {
    const next = findBuildIdentifierEndIndex(range, end + 1)
    if (next === end + 1) {
      break
    }
    end = next
  }
  return end
}

/**
 * Reads three numbers separated by dots, then any prerelease and build suffixes.
 * Uses loose rules, so forms such as 01.02.03beta can match here.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position of the first number, after any prefix.
 * @param {DigitRunCache} digitRuns Shared digit-run cache, updated as needed.
 * @returns {number} The position after the version match, or start if none matches.
 * @example findLooseFullVersionEndIndex('1.2.3-beta+sha <2', 0, digitRuns) // 14
 * @example findLooseFullVersionEndIndex('1.2', 0, digitRuns) // 0; needs three numbers
 */
const findLooseFullVersionEndIndex = (range, start, digitRuns) => {
  let end = start
  for (let component = 0; component < 3; component++) {
    const next = findVersionNumberEndIndex(range, end, true, digitRuns)
    if (next === end) {
      return start
    }
    end = next
    if (component < 2) {
      if (range[end] !== '.') {
        return start
      }
      end++
    }
  }
  const prereleaseEnd = findPrereleaseEndIndex(range, end, true, digitRuns)
  return findBuildEndIndex(range, prereleaseEnd)
}

/**
 * Reads one component of a partial or wildcard version, such as 2 or x in 2.x.
 * Numbers use strict leading-zero rules; wildcards take up one character.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position where the component should begin.
 * @param {DigitRunCache} digitRuns Shared digit-run cache, updated as needed.
 * @returns {number} The position after the component, or start if none matches.
 * @example findPartialVersionComponentEndIndex('2.x', 0, digitRuns) // 1, at the dot
 * @example findPartialVersionComponentEndIndex('2.x', 2, digitRuns) // 3, after x
 */
const findPartialVersionComponentEndIndex = (range, start, digitRuns) => {
  if (isDigit(range[start])) {
    return findVersionNumberEndIndex(range, start, false, digitRuns)
  }
  return isVersionStart(range[start]) ? start + 1 : start
}

/**
 * Reads up to three components, allowing forms such as 1, 1.2, or 1.x.
 * After the third component, also reads any strict prerelease and build suffixes.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position of a number or wildcard, after any prefix.
 * @param {DigitRunCache} digitRuns Shared digit-run cache, updated as needed.
 * @returns {number} The position after the matched version text.
 * @example findPartialVersionEndIndex('1.x <2', 0, digitRuns) // 3, at the space
 * @example findPartialVersionEndIndex('1.', 0, digitRuns) // 1; the trailing dot is unread
 */
const findPartialVersionEndIndex = (range, start, digitRuns) => {
  let end = findPartialVersionComponentEndIndex(range, start, digitRuns)
  for (let component = 0; component < 2; component++) {
    if (range[end] !== '.') {
      return end
    }
    const next = findPartialVersionComponentEndIndex(range, end + 1, digitRuns)
    if (next === end + 1) {
      return end
    }
    end = next
  }
  const prereleaseEnd = findPrereleaseEndIndex(range, end, false, digitRuns)
  return findBuildEndIndex(range, prereleaseEnd)
}

/**
 * Finds where a version match ends, without deciding whether the range is valid.
 * Tries a loose full version first, then a partial or wildcard version, just
 * like the old regex's LOOSEPLAIN and XRANGEPLAIN alternatives.
 *
 * @param {string} range The text to read; it is not changed.
 * @param {number} start The position of a number or wildcard, after any prefix.
 * @param {DigitRunCache} digitRuns Shared digit-run cache, updated as needed.
 * @returns {number} The position where scanning the next candidate can resume.
 * @example findVersionEndIndex('1.2.3 <2', 0, digitRuns) // 5, at the space
 * @example findVersionEndIndex('1.x <2', 0, digitRuns) // 3, at the space
 */
const findVersionEndIndex = (range, start, digitRuns) => {
  const fullVersionEnd = findLooseFullVersionEndIndex(range, start, digitRuns)
  return fullVersionEnd !== start
    ? fullVersionEnd
    : findPartialVersionEndIndex(range, start, digitRuns)
}

/**
 * Finds the one whitespace character the old replacement would remove.
 * Keeps leading whitespace, skips an optional operator, and checks immediately
 * after it. Returns -1 if there is nothing to remove.
 *
 * @param {string} range The text containing a candidate already found by the caller.
 * @param {number} matchStart The candidate's position, including leading whitespace.
 * @returns {number} The position of the removable whitespace, or -1 if absent.
 * @example findRemovableWhitespaceIndex(' > 1', 0) // 2; keep the space at position 0
 * @example findRemovableWhitespaceIndex('>1', 0) // -1
 */
const findRemovableWhitespaceIndex = (range, matchStart) => {
  let position = matchStart
  if (isWhitespace(range[position])) {
    position++
  }
  if (range[position] === '<' || range[position] === '>') {
    position++
  }
  if (range[position] === '=') {
    position++
  }
  return isWhitespace(range[position]) ? position : -1
}

/**
 * Joins an operator to its version, turning > 1.2.3 into >1.2.3.
 * Copies everything else according to the old regex's matching rules.
 * Moves forward through prefixes and whole versions to avoid repeatedly
 * searching the same characters on long inputs.
 *
 * @param {string} range The range text to process; the original string is unchanged.
 * @returns {string} The range with matched operator whitespace removed.
 * @example trimComparatorWhitespace('> 1.2.3 < 2') // '>1.2.3 <2'
 * @example trimComparatorWhitespace('> invalid') // '> invalid'
 */
const trimComparatorWhitespace = range => {
  let result = ''
  let copyFrom = 0
  let cursor = 0
  const digitRuns = { start: -1, end: -1, previousStart: -1, previousEnd: -1 }

  while (cursor < range.length) {
    const matchStart = cursor
    if (range[cursor] === '<' || range[cursor] === '>') {
      cursor++
    } else if (!isVersionPrefix(range[cursor]) && !isVersionStart(range[cursor])) {
      cursor++
      continue
    }

    // Consume a v/=/whitespace run once, even when no version follows it.
    while (cursor < range.length && isVersionPrefix(range[cursor])) {
      cursor++
    }
    if (!isVersionStart(range[cursor])) {
      continue
    }

    const whitespacePosition = findRemovableWhitespaceIndex(range, matchStart)
    if (whitespacePosition !== -1) {
      result += range.slice(copyFrom, whitespacePosition)
      copyFrom = whitespacePosition + 1
    }

    // Skip the entire match, so a prerelease's trailing v cannot be
    // reconsidered as the prefix of another version.
    cursor = findVersionEndIndex(range, cursor, digitRuns)
  }

  return result + range.slice(copyFrom)
}

module.exports = trimComparatorWhitespace
