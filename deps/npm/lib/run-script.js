
module.exports = runScript

var lifecycle = require("./utils/lifecycle.js")
  , npm = require("./npm.js")
  , path = require("path")
  , readJson = require("read-package-json")
  , log = require("npmlog")
  , chain = require("slide").chain
  , fs = require("graceful-fs")
  , asyncMap = require("slide").asyncMap

runScript.usage = "npm run-script [<pkg>] <command>"

runScript.completion = function (opts, cb) {

  // see if there's already a package specified.
  var argv = opts.conf.argv.remain
    , installedShallow = require("./utils/completion/installed-shallow.js")

  if (argv.length >= 4) return cb()

  if (argv.length === 3) {
    // either specified a script locally, in which case, done,
    // or a package, in which case, complete against its scripts
    var json = path.join(npm.prefix, "package.json")
    return readJson(json, function (er, d) {
      if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
      if (er) d = {}
      var scripts = Object.keys(d.scripts || {})
      console.error("local scripts", scripts)
      if (scripts.indexOf(argv[2]) !== -1) return cb()
      // ok, try to find out which package it was, then
      var pref = npm.config.get("global") ? npm.config.get("prefix")
               : npm.prefix
      var pkgDir = path.resolve( pref, "node_modules"
                               , argv[2], "package.json" )
      readJson(pkgDir, function (er, d) {
        if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
        if (er) d = {}
        var scripts = Object.keys(d.scripts || {})
        return cb(null, scripts)
      })
    })
  }

  // complete against the installed-shallow, and the pwd's scripts.
  // but only packages that have scripts
  var installed
    , scripts
  installedShallow(opts, function (d) {
    return d.scripts
  }, function (er, inst) {
    installed = inst
    next()
  })

  if (npm.config.get("global")) scripts = [], next()
  else readJson(path.join(npm.prefix, "package.json"), function (er, d) {
    if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
    d = d || {}
    scripts = Object.keys(d.scripts || {})
    next()
  })

  function next () {
    if (!installed || !scripts) return
    return cb(null, scripts.concat(installed))
  }
}

function runScript (args, cb) {
  if (!args.length) return list(cb)
  var pkgdir = args.length === 1 ? process.cwd()
             : path.resolve(npm.dir, args[0])
    , cmd = args.pop()

  readJson(path.resolve(pkgdir, "package.json"), function (er, d) {
    if (er) return cb(er)
    run(d, pkgdir, cmd, cb)
  })
}

function list(cb) {
  var json = path.join(npm.prefix, 'package.json')
  return readJson(json, function(er, d) {
    if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
    if (er) d = {}
    var scripts = Object.keys(d.scripts || {})

    if (log.level === "silent") {
      return cb(null, scripts)
    }

    if (npm.config.get("json")) {
      console.log(JSON.stringify(d.scripts || {}, null, 2))
      return cb(null, scripts)
    }

    var s = ":"
    var prefix = ""
    if (!npm.config.get("parseable")) {
      s = "\n    "
      prefix = "  "
      console.log("Available scripts in the %s package:", d.name)
    }
    scripts.forEach(function(script) {
      console.log(prefix + script + s + d.scripts[script])
    })
    return cb(null, scripts)
  })
}

function run (pkg, wd, cmd, cb) {
  var cmds = []
  if (!pkg.scripts) pkg.scripts = {}
  if (cmd === "restart") {
    cmds = ["prestop","stop","poststop"
           ,"restart"
           ,"prestart","start","poststart"]
  } else {
    cmds = [cmd]
  }
  if (!cmd.match(/^(pre|post)/)) {
    cmds = ["pre"+cmd].concat(cmds).concat("post"+cmd)
  }
  log.verbose("run-script", cmds)
  chain(cmds.map(function (c) {
    // when running scripts explicitly, assume that they're trusted.
    return [lifecycle, pkg, c, wd, true]
  }), cb)
}
