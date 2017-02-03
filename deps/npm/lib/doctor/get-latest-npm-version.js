var log = require('npmlog')
var fetchPackageMetadata = require('../fetch-package-metadata')

function getLatestNpmVersion (cb) {
  var tracker = log.newItem('getLatestNpmVersion', 1)
  tracker.info('getLatestNpmVersion', 'Getting npm package information')
  fetchPackageMetadata('npm@latest', '.', function (e, d) {
    tracker.finish()
    cb(e, d.version)
  })
}

module.exports = getLatestNpmVersion
