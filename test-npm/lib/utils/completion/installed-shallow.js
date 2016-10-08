
module.exports = installedShallow

var npm = require('../../npm.js')
var fs = require('graceful-fs')
var path = require('path')
var readJson = require('read-package-json')
var asyncMap = require('slide').asyncMap

function installedShallow (opts, filter, cb) {
  if (typeof cb !== 'function') {
    cb = filter
    filter = null
  }
  var conf = opts.conf
  var args = conf.argv.remain
  if (args.length > 3) return cb()
  var local
  var global
  var localDir = npm.dir
  var globalDir = npm.globalDir
  if (npm.config.get('global')) {
    local = []
    next()
  } else {
    fs.readdir(localDir, function (er, pkgs) {
      local = (pkgs || []).filter(function (p) {
        return p.charAt(0) !== '.'
      })
      next()
    })
  }

  fs.readdir(globalDir, function (er, pkgs) {
    global = (pkgs || []).filter(function (p) {
      return p.charAt(0) !== '.'
    })
    next()
  })
  function next () {
    if (!local || !global) return
    filterInstalled(local, global, filter, cb)
  }
}

function filterInstalled (local, global, filter, cb) {
  var fl
  var fg

  if (!filter) {
    fl = local
    fg = global
    return next()
  }

  asyncMap(local, function (p, cb) {
    readJson(path.join(npm.dir, p, 'package.json'), function (er, d) {
      if (!d || !filter(d)) return cb(null, [])
      return cb(null, d.name)
    })
  }, function (er, local) {
    fl = local || []
    next()
  })

  var globalDir = npm.globalDir
  asyncMap(global, function (p, cb) {
    readJson(path.join(globalDir, p, 'package.json'), function (er, d) {
      if (!d || !filter(d)) return cb(null, [])
      return cb(null, d.name)
    })
  }, function (er, global) {
    fg = global || []
    next()
  })

  function next () {
    if (!fg || !fl) return
    if (!npm.config.get('global')) {
      fg = fg.map(function (g) {
        return [g, '-g']
      })
    }
    console.error('filtered', fl, fg)
    return cb(null, fl.concat(fg))
  }
}
