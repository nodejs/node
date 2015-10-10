'use strict'
// var cache = require('../../cache.js')

module.exports = function (top, buildpath, pkg, log, next) {
  next()
/*
// FIXME: Unnecessary as long as we have to have the tarball to resolve all deps, which
// is progressively seeming to be likely for the indefinite future.
// ALSO fails for local deps specified with relative URLs outside of the top level.

  var name = pkg.package.name
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
  log.silly('fetch', name, version)
  cache.add(name, version, top, false, next)
*/
}
