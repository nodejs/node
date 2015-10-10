'use strict'
var path = require('path')

module.exports = function (top, buildpath, pkg, log, next) {
  log.warn('update-linked', path.relative(top, pkg.path), 'needs updating to', pkg.package.version,
    'from', pkg.oldPkg.package.version, "but we can't, as it's a symlink")
  next()
}
