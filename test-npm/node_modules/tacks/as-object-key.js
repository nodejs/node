'use strict'
module.exports = asObjectKey

var asLiteral = require('./as-literal.js')

function asObjectKey (key) {
  var isIdent = /^[a-zA-Z$_][a-zA-Z$_0-9]+$/.test(key)
  return isIdent ? key : asLiteral(key)
}
