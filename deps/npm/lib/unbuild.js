module.exports = unbuild
unbuild.usage = "npm unbuild <folder>\n(this is plumbing)"

var readJson = require("read-package-json")
  , rm = require("rimraf")
  , gentlyRm = require("./utils/gently-rm.js")
  , npm = require("./npm.js")
  , path = require("path")
  , fs = require("graceful-fs")
  , lifecycle = require("./utils/lifecycle.js")
  , asyncMap = require("slide").asyncMap
  , chain = require("slide").chain
  , log = require("npmlog")
  , build = require("./build.js")

// args is a list of folders.
// remove any bins/etc, and then delete the folder.
function unbuild (args, silent, cb) {
  if (typeof silent === 'function') cb = silent, silent = false
  asyncMap(args, unbuild_(silent), cb)
}

function unbuild_ (silent) { return function (folder, cb) {
  folder = path.resolve(folder)
  delete build._didBuild[folder]
  log.info(folder, "unbuild")
  readJson(path.resolve(folder, "package.json"), function (er, pkg) {
    // if no json, then just trash it, but no scripts or whatever.
    if (er) return rm(folder, cb)
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
        , [rm, folder] ]
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

  log.verbose([top, gnm, parent], "unbuild " + pkg._id)
  asyncMap([rmBins, rmMans], function (fn, cb) {
    fn(pkg, folder, parent, top, cb)
  }, cb)
}

function rmBins (pkg, folder, parent, top, cb) {
  if (!pkg.bin) return cb()
  var binRoot = top ? npm.bin : path.resolve(parent, ".bin")
  log.verbose([binRoot, pkg.bin], "binRoot")
  asyncMap(Object.keys(pkg.bin), function (b, cb) {
    if (process.platform === "win32") {
      chain([ [rm, path.resolve(binRoot, b) + ".cmd"]
            , [rm, path.resolve(binRoot, b) ] ], cb)
    } else {
      gentlyRm( path.resolve(binRoot, b)
              , !npm.config.get("force") && folder
              , cb )
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
  asyncMap(pkg.man, function (man, cb) {
    if (Array.isArray(man)) {
      man.forEach(rm)
    } else {
      rm(man)
    }

    function rm(man) {
      var parseMan = man.match(/(.*)\.([0-9]+)(\.gz)?$/)
        , stem = parseMan[1]
        , sxn = parseMan[2]
        , gz = parseMan[3] || ""
        , bn = path.basename(stem)
        , manDest = path.join( manRoot
                            , "man"+sxn
                            , (bn.indexOf(pkg.name) === 0 ? bn
                              : pkg.name + "-" + bn)
                              + "." + sxn + gz
                            )
      gentlyRm( manDest
              , !npm.config.get("force") && folder
              , cb )
    }
  }, cb)
}
