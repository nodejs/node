'use strict'
var path = require('path')

function getTop (pkg) {
  if (pkg.target && pkg.target.parent) return getTop(pkg.target.parent)
  if (pkg.parent) return getTop(pkg.parent)
  return pkg.path
}

module.exports = function (staging, pkg, log, next) {
  if (pkg.package.version !== pkg.oldPkg.package.version) {
    log.warn('update-linked', path.relative(getTop(pkg), pkg.path), 'needs updating to', pkg.package.version,
      'from', pkg.oldPkg.package.version, "but we can't, as it's a symlink")
  }
  next()
}
