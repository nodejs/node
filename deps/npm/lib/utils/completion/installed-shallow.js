
module.exports = installedShallow

var npm = require("../../npm.js")
  , fs = require("graceful-fs")
  , path = require("path")
  , readJson = require("read-package-json")
  , asyncMap = require("slide").asyncMap

function installedShallow (opts, filter, cb) {
  if (typeof cb !== "function") cb = filter, filter = null
  var conf = opts.conf
    , args = conf.argv.remain
  if (args.length > 3) return cb()
  var local
    , global
    , localDir = npm.dir
    , globalDir = npm.globalDir
  if (npm.config.get("global")) local = [], next()
  else fs.readdir(localDir, function (er, pkgs) {
    local = (pkgs || []).filter(function (p) {
      return p.charAt(0) !== "."
    })
    next()
  })
  fs.readdir(globalDir, function (er, pkgs) {
    global = (pkgs || []).filter(function (p) {
      return p.charAt(0) !== "."
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
    , fg

  if (!filter) {
    fl = local
    fg = global
    return next()
  }

  asyncMap(local, function (p, cb) {
    readJson(path.join(npm.dir, p, "package.json"), function (er, d) {
      if (!d || !filter(d)) return cb(null, [])
      return cb(null, d.name)
    })
  }, function (er, local) {
    fl = local || []
    next()
  })

  var globalDir = npm.globalDir
  asyncMap(global, function (p, cb) {
    readJson(path.join(globalDir, p, "package.json"), function (er, d) {
      if (!d || !filter(d)) return cb(null, [])
      return cb(null, d.name)
    })
  }, function (er, global) {
    fg = global || []
    next()
  })

  function next () {
    if (!fg || !fl) return
    if (!npm.config.get("global")) {
      fg = fg.map(function (g) {
        return [g, "-g"]
      })
    }
    console.error("filtered", fl, fg)
    return cb(null, fl.concat(fg))
  }
}
