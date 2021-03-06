'use strict'

module.exports = trimTrailingLines

// Remove final newline characters from `value`.
function trimTrailingLines(value) {
  return String(value).replace(/\n+$/, '')
}
