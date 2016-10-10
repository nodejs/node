module.exports = eventsToArray

var EE = require('events').EventEmitter
function eventsToArray (ee, ignore) {
  ignore = ignore || []
  var array = []

  ee.emit = (function (orig) {
    return function etoaWrap (ev) {
      if (ignore.indexOf(ev) === -1) {
        var l = arguments.length
        var args = new Array(l)
        for (var i = 0; i < l; i++) {
          var arg = arguments[i]
          args[i] = (arg instanceof EE) ?
            eventsToArray(arg, ignore) :
            arg
        }
        array.push(args)
      }

      return orig.apply(this, arguments)
    }
  })(ee.emit)

  return array
}
