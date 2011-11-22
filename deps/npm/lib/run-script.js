
module.exports = runScript

var lifecycle = require("./utils/lifecycle.js")
  , npm = require("./npm.js")
  , path = require("path")
  , readJson = require("./utils/read-json.js")
  , log = require("./utils/log.js")
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
      if (er) d = {}
      var scripts = Object.keys(d.scripts || {})
      console.error("local scripts", scripts)
      if (scripts.indexOf(argv[2]) !== -1) return cb()
      // ok, try to find out which package it was, then
      var pref = npm.config.get("global") ? npm.config.get("prefix")
               : npm.prefix
      var pkgDir = path.resolve( pref, "node_modules"
                               , argv[2], "package.json" )
      console.error("global?", npm.config.get("global"))
      console.error(pkgDir, "package dir")
      readJson(pkgDir, function (er, d) {
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
  var pkgdir = args.length === 1 ? process.cwd()
             : path.resolve(npm.dir, args[0])
    , cmd = args.pop()

  readJson(path.resolve(pkgdir, "package.json"), function (er, d) {
    if (er) return cb(er)
    run(d, pkgdir, cmd, cb)
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
  log.verbose(cmds, "run-script")
  chain(cmds.map(function (c) {
    // when running scripts explicitly, assume that they're trusted.
    return [lifecycle, pkg, c, wd, true]
  }), cb)
}
