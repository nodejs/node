'use strict'
var validate = require('aproba')
var npm = require('../npm.js')

module.exports = function (differences, decomposed, next) {
  validate('AAF', arguments)
  differences.forEach((action) => {
    var cmd = action[0]
    var pkg = action[1]
    switch (cmd) {
      case 'add':
        addSteps(decomposed, pkg)
        break
      case 'update':
        updateSteps(decomposed, pkg)
        break
      case 'move':
        moveSteps(decomposed, pkg)
        break
      case 'remove':
        removeSteps(decomposed, pkg)
        break
      default:
        defaultSteps(decomposed, cmd, pkg)
    }
  })
  next()
}

function addAction (decomposed, action, pkg) {
  if (decomposed.some((_) => _[0] === action && _[1] === pkg)) return
  decomposed.push([action, pkg])
}

function addSteps (decomposed, pkg) {
  if (pkg.fromBundle) {
    // make sure our source module exists to extract ourselves from
    // if we're installing our source module anyway, the duplication
    // of these steps will be elided by `addAction` automatically
    addAction(decomposed, 'fetch', pkg.fromBundle)
    addAction(decomposed, 'extract', pkg.fromBundle)
  }
  if (!pkg.fromBundle && !pkg.isLink) {
    addAction(decomposed, 'fetch', pkg)
    addAction(decomposed, 'extract', pkg)
  }
  if (!pkg.fromBundle || npm.config.get('rebuild-bundle')) {
    addAction(decomposed, 'preinstall', pkg)
    addAction(decomposed, 'build', pkg)
    addAction(decomposed, 'install', pkg)
    addAction(decomposed, 'postinstall', pkg)
  }
  if (!pkg.fromBundle || !pkg.isLink) {
    addAction(decomposed, 'finalize', pkg)
  }
  addAction(decomposed, 'refresh-package-json', pkg)
}

function updateSteps (decomposed, pkg) {
  removeSteps(decomposed, pkg.oldPkg)
  addSteps(decomposed, pkg)
}

function removeSteps (decomposed, pkg) {
  addAction(decomposed, 'unbuild', pkg)
  addAction(decomposed, 'remove', pkg)
}

function moveSteps (decomposed, pkg) {
  addAction(decomposed, 'move', pkg)
  addAction(decomposed, 'build', pkg)
  addAction(decomposed, 'install', pkg)
  addAction(decomposed, 'postinstall', pkg)
  addAction(decomposed, 'refresh-package-json', pkg)
}

function defaultSteps (decomposed, cmd, pkg) {
  addAction(decomposed, cmd, pkg)
}
