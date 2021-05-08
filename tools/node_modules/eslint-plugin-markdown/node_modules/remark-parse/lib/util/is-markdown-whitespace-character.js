'use strict'

module.exports = whitespace

var tab = 9 // '\t'
var lineFeed = 10 // '\n'
var lineTabulation = 11 // '\v'
var formFeed = 12 // '\f'
var carriageReturn = 13 // '\r'
var space = 32 // ' '

function whitespace(char) {
  /* istanbul ignore next - `number` handling for future */
  var code = typeof char === 'number' ? char : char.charCodeAt(0)

  switch (code) {
    case tab:
    case lineFeed:
    case lineTabulation:
    case formFeed:
    case carriageReturn:
    case space:
      return true
    default:
      return false
  }
}
