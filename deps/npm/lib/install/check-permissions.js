'use strict'
var path = require('path')
var log = require('npmlog')
var validate = require('aproba')
var uniq = require('lodash.uniq')
var asyncMap = require('slide').asyncMap
var npm = require('../npm.js')
var exists = require('./exists.js')
var writable = require('./writable.js')

module.exports = function (actions, next) {
  validate('AF', arguments)
  var errors = []
  asyncMap(actions, function (action, done) {
    var cmd = action[0]
    var pkg = action[1]
    switch (cmd) {
      case 'add':
        hasAnyWriteAccess(path.resolve(pkg.path, '..'), errors, done)
        break
      case 'update':
      case 'remove':
        hasWriteAccess(pkg.path, errors, andHasWriteAccess(path.resolve(pkg.path, '..'), errors, done))
        break
      case 'move':
        hasAnyWriteAccess(pkg.path, errors, andHasWriteAccess(path.resolve(pkg.fromPath, '..'), errors, done))
        break
      default:
        done()
    }
  }, function () {
    if (!errors.length) return next()
    uniq(errors.map(function (er) { return 'Missing write access to ' + er.path })).forEach(function (er) {
      log.warn('checkPermissions', er)
    })
    npm.config.get('force') ? next() : next(errors[0])
  })
}

function andHasWriteAccess (dir, errors, done) {
  validate('SAF', arguments)
  return function () {
    hasWriteAccess(dir, errors, done)
  }
}

function hasAnyWriteAccess (dir, errors, done) {
  validate('SAF', arguments)
  findNearestDir()
  function findNearestDir () {
    var nextDir = path.resolve(dir, '..')
    exists(dir, function (dirDoesntExist) {
      if (!dirDoesntExist || nextDir === dir) {
        return hasWriteAccess(dir, errors, done)
      } else {
        dir = nextDir
        findNearestDir()
      }
    })
  }
}

function hasWriteAccess (dir, errors, done) {
  validate('SAF', arguments)
  writable(dir, function (er) {
    if (er) errors.push(er)
    done()
  })
}
