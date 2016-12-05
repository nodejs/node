'use strict'
var path = require('path')
var fs = require('graceful-fs')
var iferr = require('iferr')
var inflateShrinkwrap = require('./inflate-shrinkwrap.js')
var parseJSON = require('../utils/parse-json.js')

var readShrinkwrap = module.exports = function (child, next) {
  if (child.package._shrinkwrap) return process.nextTick(next)
  fs.readFile(path.join(child.path, 'npm-shrinkwrap.json'), function (er, data) {
    if (er) {
      child.package._shrinkwrap = null
      return next()
    }
    try {
      child.package._shrinkwrap = parseJSON(data)
    } catch (ex) {
      child.package._shrinkwrap = null
      return next(ex)
    }
    return next()
  })
}

module.exports.andInflate = function (child, next) {
  readShrinkwrap(child, iferr(next, function () {
    if (child.package._shrinkwrap) {
      return inflateShrinkwrap(child, child.package._shrinkwrap.dependencies || {}, next)
    } else {
      return next()
    }
  }))
}
