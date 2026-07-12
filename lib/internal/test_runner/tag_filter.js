'use strict';
const {
  ArrayPrototypePush,
  ObjectFreeze,
  SafeSet,
  StringPrototypeToLowerCase,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const {
  emitExperimentalWarning,
} = require('internal/util');
const {
  validateArray,
  validateString,
} = require('internal/validators');

const kEmptyTagArray = ObjectFreeze([]);

/**
 * Validate and canonicalize a `tags` option value supplied to `test()`,
 * `it()`, `suite()`, or `describe()`.
 *
 * Each tag must be a non-empty string. Tags are canonicalized to lowercase
 * and deduplicated, preserving first-seen declaration order.
 *
 * Calling this with a non-empty array also emits the one-shot
 * `ExperimentalWarning` for the test-tags feature.
 * @param {unknown} tags - The value supplied by the user for `options.tags`.
 * @param {string} optName - Option name to embed in thrown error messages
 *   (typically `'options.tags'`).
 * @returns {string[]} Lowercased, deduplicated tags in declaration order.
 *   Returns an empty array when `tags` is `[]`.
 * @throws {ERR_INVALID_ARG_TYPE} If `tags` is not an array, or any element
 *   is not a string.
 * @throws {ERR_INVALID_ARG_VALUE} If any element is empty.
 */
function validateAndCanonicalizeTagValues(tags, optName) {
  validateArray(tags, optName);

  if (tags.length === 0) {
    return kEmptyTagArray;
  }

  emitExperimentalWarning('Test tags');

  const seen = new SafeSet();
  const result = [];

  for (let i = 0; i < tags.length; i++) {
    const tag = tags[i];
    const elemName = `${optName}[${i}]`;

    const lower = validateAndCanonicalizeTagFilter(tag, elemName);

    if (!seen.has(lower)) {
      seen.add(lower);
      ArrayPrototypePush(result, lower);
    }
  }

  return ObjectFreeze(result);
}

/**
 * Validate and canonicalize a single tag-filter string supplied via
 * `--experimental-test-tag-filter` or `testTagFilters`.
 *
 * Filters are literal tag names: a test passes the filter when its tag set
 * contains the lowercased filter value. Multi-occurrence composes by AND
 * at the call site (see `evaluateTagFilters`).
 * @param {unknown} value - The supplied filter string.
 * @param {string} name - Argument name embedded in thrown error messages.
 * @returns {string} The lowercased filter value.
 * @throws {ERR_INVALID_ARG_TYPE} If `value` is not a string.
 * @throws {ERR_INVALID_ARG_VALUE} If `value` is empty.
 */
function validateAndCanonicalizeTagFilter(value, name) {
  validateString(value, name);

  if (value.length === 0) {
    throw new ERR_INVALID_ARG_VALUE(name, value, 'must not be empty');
  }

  return StringPrototypeToLowerCase(value);
}

/**
 * Evaluate a list of tag filters against a test's tag set.
 *
 * Multiple filters compose by AND: a test passes only when every filter is
 * present in its tag set. An empty `filters` array always passes. Untagged
 * tests have an empty tag set and so are excluded under any non-empty
 * filter.
 * @param {string[]} filters - Lowercased canonical filter values.
 * @param {SafeSet<string>} tags - The test's lowercased canonical tag set.
 * @returns {boolean} `true` if the test should run.
 */
function evaluateTagFilters(filters, tags) {
  for (let i = 0; i < filters.length; i++) {
    if (!tags.has(filters[i])) {
      return false;
    }
  }
  return true;
}

module.exports = {
  evaluateTagFilters,
  kEmptyTagArray,
  validateAndCanonicalizeTagFilter,
  validateAndCanonicalizeTagValues,
};
