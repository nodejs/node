'use strict'

const assert = require('node:assert')
const { URLSerializer } = require('../fetch/data-url')
const { isValidHeaderName } = require('../fetch/util')

/**
 * @see https://url.spec.whatwg.org/#concept-url-equals
 * @param {URL} A
 * @param {URL} B
 * @param {boolean | undefined} excludeFragment
 * @returns {boolean}
 */
function urlEquals (A, B, excludeFragment = false) {
  const serializedA = URLSerializer(A, excludeFragment)

  const serializedB = URLSerializer(B, excludeFragment)

  return serializedA === serializedB
}

/**
 * @see https://github.com/chromium/chromium/blob/694d20d134cb553d8d89e5500b9148012b1ba299/content/browser/cache_storage/cache_storage_cache.cc#L260-L262
 * @param {string} header
 */
function getFieldValues (header) {
  assert(header !== null)

  const values = []

  for (let value of header.split(',')) {
    value = value.trim()

    if (isValidHeaderName(value)) {
      values.push(value)
    }
  }

  return values
}

module.exports = {
  urlEquals,
  getFieldValues
}
