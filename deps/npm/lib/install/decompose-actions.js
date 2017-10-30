'use strict'
var validate = require('aproba')
var asyncMap = require('slide').asyncMap
var npm = require('../npm.js')

module.exports = function (differences, decomposed, next) {
  validate('AAF', arguments)
  asyncMap(differences, function (action, done) {
    var cmd = action[0]
    var pkg = action[1]
    switch (cmd) {
      case 'add':
        addSteps(decomposed, pkg, done)
        break
      case 'update':
        updateSteps(decomposed, pkg, done)
        break
      case 'move':
        moveSteps(decomposed, pkg, done)
        break
      case 'remove':
        removeSteps(decomposed, pkg, done)
        break
      default:
        defaultSteps(decomposed, cmd, pkg, done)
    }
  }, next)
}

function addSteps (decomposed, pkg, done) {
  if (!pkg.fromBundle && !pkg.isLink) {
    decomposed.push(['fetch', pkg])
    decomposed.push(['extract', pkg])
  }
  if (!pkg.fromBundle || npm.config.get('rebuild-bundle')) {
    decomposed.push(['preinstall', pkg])
    decomposed.push(['build', pkg])
    decomposed.push(['install', pkg])
    decomposed.push(['postinstall', pkg])
  }
  if (!pkg.fromBundle || !pkg.isLink) {
    decomposed.push(['finalize', pkg])
  }
  decomposed.push(['refresh-package-json', pkg])
  done()
}

function updateSteps (decomposed, pkg, done) {
  removeSteps(decomposed, pkg.oldPkg, () => {
    addSteps(decomposed, pkg, done)
  })
}

function removeSteps (decomposed, pkg, done) {
  decomposed.push(['unbuild', pkg])
  decomposed.push(['remove', pkg])
  done()
}

function moveSteps (decomposed, pkg, done) {
  decomposed.push(['move', pkg])
  decomposed.push(['build', pkg])
  decomposed.push(['install', pkg])
  decomposed.push(['postinstall', pkg])
  decomposed.push(['refresh-package-json', pkg])
  done()
}

function defaultSteps (decomposed, cmd, pkg, done) {
  decomposed.push([cmd, pkg])
  done()
}
