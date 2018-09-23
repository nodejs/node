'use strict'
var path = require('path')
var validate = require('aproba')
var log = require('npmlog')
var packageId = require('../utils/package-id.js')

module.exports = function (top, differences, next) {
  validate('SAF', arguments)
  var action
  var keep = []

  differences.forEach(function (action) {
    var cmd = action[0]
    var pkg = action[1]
    if (cmd === 'remove') {
      pkg.removing = true
    }
  })

  /*eslint no-cond-assign:0*/
  while (action = differences.shift()) {
    var cmd = action[0]
    var pkg = action[1]
    if (pkg.isInLink || pkg.parent.target || pkg.parent.isLink) {
      // we want to skip warning if this is a child of another module that we're removing
      if (!pkg.parent.removing) {
        log.warn('skippingAction', 'Module is inside a symlinked module: not running ' +
          cmd + ' ' + packageId(pkg) + ' ' + path.relative(top, pkg.path))
      }
    } else {
      keep.push(action)
    }
  }
  differences.push.apply(differences, keep)
  next()
}
