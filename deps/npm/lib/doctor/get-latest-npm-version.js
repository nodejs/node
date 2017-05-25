var log = require('npmlog')
var fetchPackageMetadata = require('../fetch-package-metadata')

function getLatestNpmVersion (cb) {
  var tracker = log.newItem('getLatestNpmVersion', 1)
  tracker.info('getLatestNpmVersion', 'Getting npm package information')
  fetchPackageMetadata('npm@latest', '.', {}, function (err, d) {
    tracker.finish()
    if (err) { return cb(err) }
    cb(null, d.version)
  })
}

module.exports = getLatestNpmVersion
