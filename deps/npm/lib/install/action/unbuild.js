'use strict'
var Bluebird = require('bluebird')
var lifecycle = Bluebird.promisify(require('../../utils/lifecycle.js'))
var packageId = require('../../utils/package-id.js')
var rmStuff = Bluebird.promisify(require('../../unbuild.js').rmStuff)

module.exports = function (staging, pkg, log) {
  log.silly('unbuild', packageId(pkg))
  return lifecycle(pkg.package, 'preuninstall', pkg.path, { failOk: true }).then(() => {
    return lifecycle(pkg.package, 'uninstall', pkg.path, { failOk: true })
  }).then(() => {
    return rmStuff(pkg.package, pkg.path)
  }).then(() => {
    return lifecycle(pkg.package, 'postuninstall', pkg.path, { failOk: true })
  })
}
