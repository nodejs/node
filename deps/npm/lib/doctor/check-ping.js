var log = require('npmlog')
var ping = require('../ping.js')

function checkPing (cb) {
  var tracker = log.newItem('checkPing', 1)
  tracker.info('checkPing', 'Pinging registry')
  ping({}, true, (err, pong) => {
    if (err && err.code && err.code.match(/^E\d{3}$/)) {
      return cb(null, [err.code.substr(1)])
    } else {
      cb(null, [200, 'ok'])
    }
  })
}

module.exports = checkPing
