module.exports = unbuild
unbuild.usage = "npm unbuild <folder>\n(this is plumbing)"

var readJson = require("read-package-json")
  , gentlyRm = require("./utils/gently-rm.js")
  , npm = require("./npm.js")
  , path = require("path")
  , isInside = require("path-is-inside")
  , lifecycle = require("./utils/lifecycle.js")
  , asyncMap = require("slide").asyncMap
  , chain = require("slide").chain
  , log = require("npmlog")
  , build = require("./build.js")

// args is a list of folders.
// remove any bins/etc, and then delete the folder.
function unbuild (args, silent, cb) {
  if (typeof silent === "function") cb = silent, silent = false
  asyncMap(args, unbuild_(silent), cb)
}

function unbuild_ (silent) { return function (folder, cb_) {
  function cb (er) {
    cb_(er, path.relative(npm.root, folder))
  }
  folder = path.resolve(folder)
  var base = isInside(folder, npm.prefix) ? npm.prefix : folder
  delete build._didBuild[folder]
  log.verbose("unbuild", folder.substr(npm.prefix.length + 1))
  readJson(path.resolve(folder, "package.json"), function (er, pkg) {
    // if no json, then just trash it, but no scripts or whatever.
    if (er) return gentlyRm(folder, false, base, cb)
    readJson.cache.del(folder)
    chain
      ( [ [lifecycle, pkg, "preuninstall", folder, false, true]
        , [lifecycle, pkg, "uninstall", folder, false, true]
        , !silent && function(cb) {
            console.log("unbuild " + pkg._id)
            cb()
          }
        , [rmStuff, pkg, folder]
        , [lifecycle, pkg, "postuninstall", folder, false, true]
        , [gentlyRm, folder, false, base] ]
      , cb )
  })
}}

function rmStuff (pkg, folder, cb) {
  // if it's global, and folder is in {prefix}/node_modules,
  // then bins are in {prefix}/bin
  // otherwise, then bins are in folder/../.bin
  var parent = path.dirname(folder)
    , gnm = npm.dir
    , top = gnm === parent

  readJson.cache.del(path.resolve(folder, "package.json"))

  log.verbose("unbuild rmStuff", pkg._id, "from", gnm)
  if (!top) log.verbose("unbuild rmStuff", "in", parent)
  asyncMap([rmBins, rmMans], function (fn, cb) {
    fn(pkg, folder, parent, top, cb)
  }, cb)
}

function rmBins (pkg, folder, parent, top, cb) {
  if (!pkg.bin) return cb()
  var binRoot = top ? npm.bin : path.resolve(parent, ".bin")
  asyncMap(Object.keys(pkg.bin), function (b, cb) {
    if (process.platform === "win32") {
      chain([ [gentlyRm, path.resolve(binRoot, b) + ".cmd", true]
            , [gentlyRm, path.resolve(binRoot, b), true] ], cb)
    } else {
      gentlyRm(path.resolve(binRoot, b), true, cb)
    }
  }, cb)
}

function rmMans (pkg, folder, parent, top, cb) {
  if (!pkg.man
      || !top
      || process.platform === "win32"
      || !npm.config.get("global")) {
    return cb()
  }
  var manRoot = path.resolve(npm.config.get("prefix"), "share", "man")
  log.verbose("rmMans", "man files are", pkg.man, "in", manRoot)
  asyncMap(pkg.man, function (man, cb) {
    if (Array.isArray(man)) {
      man.forEach(rmMan)
    } else {
      rmMan(man)
    }

    function rmMan (man) {
      log.silly("rmMan", "preparing to remove", man)
      var parseMan = man.match(/(.*\.([0-9]+)(\.gz)?)$/)
      if (!parseMan) {
        log.error(
          "rmMan", man, "is not a valid name for a man file.",
          "Man files must end with a number, " +
          "and optionally a .gz suffix if they are compressed."
        )
        return cb()
      }

      var stem = parseMan[1]
      var sxn = parseMan[2]
      var gz = parseMan[3] || ""
      var bn = path.basename(stem)
      var manDest = path.join(
        manRoot,
        "man"+sxn,
        (bn.indexOf(pkg.name) === 0 ? bn : pkg.name+"-"+bn)+"."+sxn+gz
      )
      gentlyRm(manDest, true, cb)
    }
  }, cb)
}
