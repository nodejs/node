
module.exports = rebuild

var readInstalled = require("read-installed")
  , semver = require("semver")
  , log = require("npmlog")
  , path = require("path")
  , npm = require("./npm.js")
  , asyncMap = require("slide").asyncMap
  , fs = require("graceful-fs")

rebuild.usage = "npm rebuild [<name>[@<version>] [name[@<version>] ...]]"

rebuild.completion = require("./utils/completion/installed-deep.js")

function rebuild (args, cb) {
  var opt = { depth: npm.config.get("depth") }
  readInstalled(npm.prefix, opt, function (er, data) {
    log.info("readInstalled", typeof data)
    if (er) return cb(er)
    var set = filter(data, args)
      , folders = Object.keys(set).filter(function (f) {
          return f !== npm.prefix
        })
    if (!folders.length) return cb()
    log.silly("rebuild set", folders)
    cleanBuild(folders, set, cb)
  })
}

function cleanBuild (folders, set, cb) {
  npm.commands.build(folders, function (er) {
    if (er) return cb(er)
    console.log(folders.map(function (f) {
      return set[f] + " " + f
    }).join("\n"))
    cb()
  })
}

function filter (data, args, set, seen) {
  if (!set) set = {}
  if (!seen) seen = {}
  if (set.hasOwnProperty(data.path)) return set
  if (seen.hasOwnProperty(data.path)) return set
  seen[data.path] = true
  var pass
  if (!args.length) pass = true // rebuild everything
  else if (data.name && data._id) {
    for (var i = 0, l = args.length; i < l; i ++) {
      var arg = args[i]
        , nv = arg.split("@")
        , n = nv.shift()
        , v = nv.join("@")
      if (n !== data.name) continue
      if (!semver.satisfies(data.version, v, true)) continue
      pass = true
      break
    }
  }
  if (pass && data._id) {
    log.verbose("rebuild", "path, id", [data.path, data._id])
    set[data.path] = data._id
  }
  // need to also dive through kids, always.
  // since this isn't an install these won't get auto-built unless
  // they're not dependencies.
  Object.keys(data.dependencies || {}).forEach(function (d) {
    // return
    var dep = data.dependencies[d]
    if (typeof dep === "string") return
    filter(dep, args, set, seen)
  })
  return set
}
