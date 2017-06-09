var log = require('npmlog')
var request = require('request')

function getLatestNodejsVersion (url, cb) {
  var tracker = log.newItem('getLatestNodejsVersion', 1)
  tracker.info('getLatestNodejsVersion', 'Getting Node.js release information')
  var version = ''
  url = url || 'https://nodejs.org/dist/index.json'
  request(url, function (e, res, index) {
    tracker.finish()
    if (e) return cb(e)
    if (res.statusCode !== 200) {
      return cb(new Error('Status not 200, ' + res.statusCode))
    }
    try {
      JSON.parse(index).forEach(function (item) {
        if (item.lts && item.version > version) version = item.version
      })
      cb(null, version)
    } catch (e) {
      cb(e)
    }
  })
}

module.exports = getLatestNodejsVersion
