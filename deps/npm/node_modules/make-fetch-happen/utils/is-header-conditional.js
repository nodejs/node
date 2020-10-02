'use strict'

module.exports = function isHeaderConditional (headers) {
  if (!headers || typeof headers !== 'object') {
    return false
  }

  const modifiers = [
    'if-modified-since',
    'if-none-match',
    'if-unmodified-since',
    'if-match',
    'if-range'
  ]

  return Object.keys(headers)
    .some(h => modifiers.indexOf(h.toLowerCase()) !== -1)
}
