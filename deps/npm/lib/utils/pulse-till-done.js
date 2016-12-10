'use strict'
var validate = require('aproba')
var log = require('npmlog')

var pulsers = 0
var pulse

module.exports = function (prefix, cb) {
  validate('SF', [prefix, cb])
  if (!prefix) prefix = 'network'
  if (!pulsers++) {
    pulse = setInterval(function () {
      log.gauge.pulse(prefix)
    }, 250)
  }
  return function () {
    if (!--pulsers) {
      clearInterval(pulse)
    }
    cb.apply(null, arguments)
  }
}
