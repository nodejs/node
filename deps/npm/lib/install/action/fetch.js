'use strict'
// var cache = require('../../cache.js')
// var packageId = require('../../utils/package-id.js')
// var moduleName = require('../../utils/module-name.js')

module.exports = function (staging, pkg, log, next) {
  next()
/*
// FIXME: Unnecessary as long as we have to have the tarball to resolve all deps, which
// is progressively seeming to be likely for the indefinite future.
// ALSO fails for local deps specified with relative URLs outside of the top level.

  var name = moduleName(pkg)
  var version
  switch (pkg.package._requested.type) {
    case 'version':
    case 'range':
      version = pkg.package.version
      break
    case 'hosted':
      name = name + '@' + pkg.package._requested.spec
      break
    default:
      name = pkg.package._requested.raw
  }
  log.silly('fetch', packageId(pkg))
  cache.add(name, version, pkg.parent.path, false, next)
*/
}
