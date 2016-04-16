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
      case 'update':
        addSteps(decomposed, pkg, done)
        break
      case 'move':
        moveSteps(decomposed, pkg, done)
        break
      case 'remove':
      case 'update-linked':
      default:
        defaultSteps(decomposed, cmd, pkg, done)
    }
  }, next)
}

function addSteps (decomposed, pkg, done) {
  if (!pkg.fromBundle) {
    decomposed.push(['fetch', pkg])
    decomposed.push(['extract', pkg])
    decomposed.push(['test', pkg])
  }
  if (!pkg.fromBundle || npm.config.get('rebuild-bundle')) {
    decomposed.push(['preinstall', pkg])
    decomposed.push(['build', pkg])
    decomposed.push(['install', pkg])
    decomposed.push(['postinstall', pkg])
  }
  decomposed.push(['finalize', pkg])
  done()
}

function moveSteps (decomposed, pkg, done) {
  decomposed.push(['move', pkg])
  decomposed.push(['build', pkg])
  decomposed.push(['install', pkg])
  decomposed.push(['postinstall', pkg])
  decomposed.push(['test', pkg])
  done()
}

function defaultSteps (decomposed, cmd, pkg, done) {
  decomposed.push([cmd, pkg])
  done()
}
