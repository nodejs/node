'use strict'
var chain = require('slide').chain
var lifecycle = require('../../utils/lifecycle.js')
var packageId = require('../../utils/package-id.js')
var prepublishWarning = require('../../utils/warn-deprecated.js')('prepublish-on-install')

module.exports = function (top, buildpath, pkg, log, next) {
  log.silly('prepublish', packageId(pkg), buildpath)
  // TODO: for `npm@5`, change the behavior and remove this warning.
  // see https://github.com/npm/npm/issues/10074 for details
  if (pkg.package && pkg.package.scripts && pkg.package.scripts.prepublish) {
    prepublishWarning([
      'As of npm@5, `prepublish` scripts will run only for `npm publish`.',
      '(In npm@4 and previous versions, it also runs for `npm install`.)',
      'See the deprecation note in `npm help scripts` for more information.'
    ])
  }
  chain(
    [
      [lifecycle, pkg.package, 'prepublish', buildpath, false, false],
      [lifecycle, pkg.package, 'prepare', buildpath, false, false]
    ],
    next
  )
}
