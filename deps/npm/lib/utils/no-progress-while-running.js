var log = require('npmlog')
var progressEnabled
var running = 0

var startRunning = exports.startRunning = function () {
  if (progressEnabled == null)
    progressEnabled = log.progressEnabled
  if (progressEnabled)
    log.disableProgress()
  ++running
}

var stopRunning = exports.stopRunning = function () {
  --running
  if (progressEnabled && running === 0)
    log.enableProgress()
}

exports.tillDone = function noProgressTillDone (cb) {
  startRunning()
  return function () {
    stopRunning()
    cb.apply(this, arguments)
  }
}
