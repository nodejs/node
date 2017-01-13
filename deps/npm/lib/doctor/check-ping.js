var log = require('npmlog')
var ping = require('../ping.js')

function checkPing (cb) {
  var tracker = log.newItem('checkPing', 1)
  tracker.info('checkPing', 'Pinging registry')
  ping({}, true, function (err, pong) {
    tracker.finish()
    cb(err, pong)
  })
}

module.exports = checkPing
