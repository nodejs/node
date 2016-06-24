'use strict'
var util = require('util')

module.exports = function (obj, event, next) {
  var timeout = setTimeout(gotTimeout, 10)
  obj.once(event, gotResult)

  function gotTimeout () {
    obj.removeListener(event, gotResult)
    next(new Error('Timeout listening for ' + event))
  }
  var result = []
  function gotResult () {
    result = Array.prototype.slice.call(arguments)
    clearTimeout(timeout)
    timeout = setTimeout(gotNoMoreResults, 10)
    obj.once(event, gotTooManyResults)
  }
  function gotNoMoreResults () {
    obj.removeListener(event, gotTooManyResults)
    var args = [null].concat(result)
    next.apply(null, args)
  }
  function gotTooManyResults () {
    var secondResult = Array.prototype.slice.call(arguments)
    clearTimeout(timeout)
    next(new Error('Got too many results, first ' + util.inspect(result) + ' and then ' + util.inspect(secondResult)))
  }
}
