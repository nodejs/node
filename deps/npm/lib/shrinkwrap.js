// emit JSON describing versions of all packages currently installed (for later
// use with shrinkwrap install)

module.exports = exports = shrinkwrap

var npm = require("./npm.js")
  , log = require("npmlog")
  , fs = require("fs")
  , path = require("path")
  , readJson = require("read-package-json")
  , sortedObject = require("sorted-object")

shrinkwrap.usage = "npm shrinkwrap"

function shrinkwrap (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false

  if (args.length) {
    log.warn("shrinkwrap", "doesn't take positional args")
  }

  npm.commands.ls([], true, function (er, _, pkginfo) {
    if (er) return cb(er)
    shrinkwrap_(pkginfo, silent, npm.config.get("dev"), cb)
  })
}

function shrinkwrap_ (pkginfo, silent, dev, cb) {
  if (pkginfo.problems) {
    return cb(new Error("Problems were encountered\n"
                       +"Please correct and try again.\n"
                       +pkginfo.problems.join("\n")))
  }

  if (!dev) {
    // remove dev deps unless the user does --dev
    readJson(path.resolve(npm.prefix, "package.json"), function (er, data) {
      if (er)
        return cb(er)
      if (data.devDependencies) {
        Object.keys(data.devDependencies).forEach(function (dep) {
          if (data.dependencies && data.dependencies[dep]) {
            // do not exclude the dev dependency if it's also listed as a dependency
            return
          }

          log.warn("shrinkwrap", "Excluding devDependency: %s", dep)
          delete pkginfo.dependencies[dep]
        })
      }
      save(pkginfo, silent, cb)
    })
  } else {
    save(pkginfo, silent, cb)
  }
}


function save (pkginfo, silent, cb) {
  // copy the keys over in a well defined order
  // because javascript objects serialize arbitrarily
  pkginfo.dependencies = sortedObject(pkginfo.dependencies || {})
  var swdata
  try {
    swdata = JSON.stringify(pkginfo, null, 2) + "\n"
  } catch (er) {
    log.error("shrinkwrap", "Error converting package info to json")
    return cb(er)
  }

  var file = path.resolve(npm.prefix, "npm-shrinkwrap.json")

  fs.writeFile(file, swdata, function (er) {
    if (er) return cb(er)
    if (silent) return cb(null, pkginfo)
    console.log("wrote npm-shrinkwrap.json")
    cb(null, pkginfo)
  })
}
