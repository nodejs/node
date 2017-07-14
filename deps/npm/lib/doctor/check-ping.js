var log = require('npmlog')
var ping = require('../ping.js')

function checkPing (cb) {
  var tracker = log.newItem('checkPing', 1)
  tracker.info('checkPing', 'Pinging registry')
  ping({}, true, (_err, pong, data, res) => {
    cb(null, [res.statusCode, res.statusMessage])
  })
}

module.exports = checkPing
