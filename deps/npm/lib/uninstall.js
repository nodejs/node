
// remove a package.

module.exports = uninstall

uninstall.usage = "npm uninstall <name>[@<version> [<name>[@<version>] ...]"
                + "\nnpm rm <name>[@<version> [<name>[@<version>] ...]"

uninstall.completion = require("./utils/completion/installed-shallow.js")

var fs = require("graceful-fs")
  , log = require("npmlog")
  , readJson = require("read-package-json")
  , path = require("path")
  , npm = require("./npm.js")
  , asyncMap = require("slide").asyncMap

function uninstall (args, cb) {
  // this is super easy
  // get the list of args that correspond to package names in either
  // the global npm.dir,
  // then call unbuild on all those folders to pull out their bins
  // and mans and whatnot, and then delete the folder.

  var nm = npm.dir
  if (args.length === 1 && args[0] === ".") args = []
  if (args.length) return uninstall_(args, nm, cb)

  // remove this package from the global space, if it's installed there
  if (npm.config.get("global")) return cb(uninstall.usage)
  readJson(path.resolve(npm.prefix, "package.json"), function (er, pkg) {
    if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
    if (er) return cb(uninstall.usage)
    uninstall_( [pkg.name]
              , npm.dir
              , cb )
  })

}

function uninstall_ (args, nm, cb) {
  // if we've been asked to --save or --save-dev or --save-optional,
  // then also remove it from the associated dependencies hash.
  var s = npm.config.get('save')
    , d = npm.config.get('save-dev')
    , o = npm.config.get('save-optional')
  if (s || d || o) {
    cb = saver(args, nm, cb)
  }

  asyncMap(args, function (arg, cb) {
    // uninstall .. should not delete /usr/local/lib/node_modules/..
    var p = path.join(path.resolve(nm), path.join("/", arg))
    if (path.resolve(p) === nm) {
      log.warn("uninstall", "invalid argument: %j", arg)
      return cb(null, [])
    }
    fs.lstat(p, function (er) {
      if (er) {
        log.warn("uninstall", "not installed in %s: %j", nm, arg)
        return cb(null, [])
      }
      cb(null, p)
    })
  }, function (er, folders) {
    if (er) return cb(er)
    asyncMap(folders, npm.commands.unbuild, cb)
  })
}

function saver (args, nm, cb_) {
  return cb
  function cb (er, data) {
    var s = npm.config.get('save')
      , d = npm.config.get('save-dev')
      , o = npm.config.get('save-optional')
    if (er || !(s || d || o)) return cb_(er, data)
    var pj = path.resolve(nm, '..', 'package.json')
    // don't use readJson here, because we don't want all the defaults
    // filled in, for mans and other bs.
    fs.readFile(pj, 'utf8', function (er, json) {
      try {
        var pkg = JSON.parse(json)
      } catch (_) {}
      if (!pkg) return cb_(null, data)

      var bundle
      if (npm.config.get('save-bundle')) {
        var bundle = pkg.bundleDependencies || pkg.bundledDependencies
        if (!Array.isArray(bundle)) bundle = undefined
      }

      var changed = false
      args.forEach(function (a) {
        ; [ [s, 'dependencies']
          , [o, 'optionalDependencies']
          , [d, 'devDependencies'] ].forEach(function (f) {
            var flag = f[0]
              , field = f[1]
            if (!flag || !pkg[field] || !pkg[field].hasOwnProperty(a)) return
            changed = true

            if (bundle) {
              var i = bundle.indexOf(a)
              if (i !== -1) bundle.splice(i, 1)
            }

            delete pkg[field][a]
          })
      })
      if (!changed) return cb_(null, data)

      if (bundle) {
        delete pkg.bundledDependencies
        if (bundle.length) {
          pkg.bundleDependencies = bundle
        } else {
          delete pkg.bundleDependencies
        }
      }

      fs.writeFile(pj, JSON.stringify(pkg, null, 2) + "\n", function (er) {
        return cb_(er, data)
      })
    })
  }
}
