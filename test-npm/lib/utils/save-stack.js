'use strict'
var inherits = require('inherits')

module.exports = SaveStack

function SaveStack (fn) {
  Error.call(this)
  Error.captureStackTrace(this, fn || SaveStack)
}
inherits(SaveStack, Error)

SaveStack.prototype.completeWith = function (er) {
  this['__' + 'proto' + '__'] = er
  this.stack = this.stack + '\n\n' + er.stack
  return this
}
