// emit JSON describing versions of all packages currently installed (for later
// use with shrinkwrap install)

module.exports = exports = shrinkwrap

var npm = require("./npm.js")
  , output = require("./utils/output.js")
  , log = require("./utils/log.js")
  , fs = require("fs")
  , path = require("path")

shrinkwrap.usage = "npm shrinkwrap"

function shrinkwrap (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false

  if (args.length) {
    log.warn("shrinkwrap doesn't take positional args.")
  }

  npm.commands.ls([], true, function (er, _, pkginfo) {
    if (er) return cb(er)
    shrinkwrap_(pkginfo, silent, cb)
  })
}

function shrinkwrap_ (pkginfo, silent, cb) {
  if (pkginfo.problems) {
    return cb(new Error("Problems were encountered\n"
                       +"Please correct and try again.\n"
                       +pkginfo.problems.join("\n")))
  }
  try {
    var swdata = JSON.stringify(pkginfo, null, 2) + "\n"
  } catch (er) {
    log.error("Error converting package info to json")
    return cb(er)
  }

  var file = path.resolve(npm.prefix, "npm-shrinkwrap.json")

  fs.writeFile(file, swdata, function (er) {
    if (er) return cb(er)
    if (silent) return cb(null, pkginfo)
    output.write("wrote npm-shrinkwrap.json", function (er) {
      cb(er, pkginfo)
    })
  })
}
