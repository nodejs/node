// npm submodule <pkg>
// Check the package contents for a git repository url.
// If there is one, then create a git submodule in the node_modules folder.

module.exports = submodule

var npm = require("./npm.js")
  , exec = require("child_process").execFile
  , cache = require("./cache.js")
  , asyncMap = require("slide").asyncMap
  , chain = require("slide").chain
  , which = require("which")

submodule.usage = "npm submodule <pkg>"

submodule.completion = require("./docs.js").completion

function submodule (args, cb) {
  if (npm.config.get("global")) {
    return cb(new Error("Cannot use submodule command in global mode."))
  }

  if (args.length === 0) return cb(submodule.usage)

  asyncMap(args, function (arg, cb) {
    cache.add(arg, cb)
  }, function (er, pkgs) {
    if (er) return cb(er)
    chain(pkgs.map(function (pkg) { return function (cb) {
      submodule_(pkg, cb)
    }}), cb)
  })

}

function submodule_ (pkg, cb) {
  if (!pkg.repository
      || pkg.repository.type !== "git"
      || !pkg.repository.url) {
    return cb(new Error(pkg._id + ": No git repository listed"))
  }

  // prefer https:// github urls
  pkg.repository.url = pkg.repository.url
    .replace(/^(git:\/\/)?(git@)?github.com[:\/]/, "https://github.com/")

  // first get the list of submodules, and update if it's already there.
  getSubmodules(function (er, modules) {
    if (er) return cb(er)
    // if there's already a submodule, then just update it.
    if (modules.indexOf(pkg.name) !== -1) {
      return updateSubmodule(pkg.name, cb)
    }
    addSubmodule(pkg.name, pkg.repository.url, cb)
  })
}

function updateSubmodule (name, cb) {
  var git = npm.config.get("git")
  var args = [ "submodule", "update", "--init", "node_modules/", name ]

  // check for git
  which(git, function (err) {
    if (err) {
      err.code = "ENOGIT"
      return cb(err)
    }

    exec(git, args, cb)
  })
}

function addSubmodule (name, url, cb) {
  var git = npm.config.get("git")
  var args = [ "submodule", "add", url, "node_modules/", name ]

  // check for git
  which(git, function (err) {
    if (err) {
      err.code = "ENOGIT"
      return cb(err)
    }

    exec(git, args, function (er) {
      if (er) return cb(er)
      updateSubmodule(name, cb)
    })
  })
}


var getSubmodules = function getSubmodules (cb) {
  var git = npm.config.get("git")
  var args = [ "submodule", "status" ]

  // check for git
  which(git, function (err) {
    if (err) {
      err.code = "ENOGIT"
      return cb(err)
    }
    exec(git, args, function (er, stdout, stderr) {
      if (er) return cb(er)
      res = stdout.trim().split(/\n/).map(function (line) {
        return line.trim().split(/\s+/)[1]
      }).filter(function (line) {
        // only care about submodules in the node_modules folder.
        return line && line.match(/^node_modules\//)
      }).map(function (line) {
        return line.replace(/^node_modules\//g, "")
      })

      // memoize.
      getSubmodules = function (cb) { return cb(null, res) }

      cb(null, res)
    })
  })
}
