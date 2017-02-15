'use strict'
var log = require('npmlog')
// output to stdout in a progress bar compatible way
module.exports = function () {
  log.clearProgress()
  console.log.apply(console, arguments)
  log.showProgress()
}
