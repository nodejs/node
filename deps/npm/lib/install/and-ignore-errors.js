'use strict'

module.exports = function (cb) {
  return function () {
    var args = Array.prototype.slice.call(arguments, 1)
    if (args.length) args.unshift(null)
    return cb.apply(null, args)
  }
}
