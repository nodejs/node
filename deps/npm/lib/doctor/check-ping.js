var log = require('npmlog')
var ping = require('../ping.js')

function checkPing (cb) {
  var tracker = log.newItem('checkPing', 1)
  tracker.info('checkPing', 'Pinging registry')
  ping({}, true, function (err, pong, data, res) {
    if (err) { return cb(err) }
    tracker.finish()
    cb(null, [res.statusCode, res.statusMessage])
  })
}

module.exports = checkPing
