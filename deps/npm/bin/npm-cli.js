#!/usr/bin/env node
;(function () { // wrapper in case we're in module_context mode

// windows: running "npm blah" in this folder will invoke WSH, not node.
if (typeof WScript !== "undefined") {
  WScript.echo("npm does not work when run\n"
              +"with the Windows Scripting Host\n\n"
              +"'cd' to a different directory,\n"
              +"or type 'npm.cmd <args>',\n"
              +"or type 'node npm <args>'.")
  WScript.quit(1)
  return
}


process.title = "npm"

var log = require("../lib/utils/log.js")
log.waitForConfig()
log.info("ok", "it worked if it ends with")

var fs = require("graceful-fs")
  , path = require("path")
  , npm = require("../lib/npm.js")
  , ini = require("../lib/utils/ini.js")
  , errorHandler = require("../lib/utils/error-handler.js")

  , configDefs = require("../lib/utils/config-defs.js")
  , shorthands = configDefs.shorthands
  , types = configDefs.types
  , nopt = require("nopt")

// if npm is called as "npmg" or "npm_g", then
// run in global mode.
if (path.basename(process.argv[1]).slice(-1)  === "g") {
  process.argv.splice(1, 1, "npm", "-g")
}

log.verbose(process.argv, "cli")

var conf = nopt(types, shorthands)
npm.argv = conf.argv.remain
if (npm.deref(npm.argv[0])) npm.command = npm.argv.shift()
else conf.usage = true


if (conf.version) {
  console.log(npm.version)
  return
}

log.info("npm@"+npm.version, "using")
log.info("node@"+process.version, "using")

// make sure that this version of node works with this version of npm.
var semver = require("semver")
  , nodeVer = process.version
  , reqVer = npm.nodeVersionRequired
if (reqVer && !semver.satisfies(nodeVer, reqVer)) {
  return errorHandler(new Error(
    "npm doesn't work with node " + nodeVer
    + "\nRequired: node@" + reqVer), true)
}

process.on("uncaughtException", errorHandler)

if (conf.usage && npm.command !== "help") {
  npm.argv.unshift(npm.command)
  npm.command = "help"
}

// now actually fire up npm and run the command.
// this is how to use npm programmatically:
conf._exit = true
npm.load(conf, function (er) {
  if (er) return errorHandler(er)
  npm.commands[npm.command](npm.argv, errorHandler)
})

})()
