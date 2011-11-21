
module.exports = loadPackageDefaults

var path = require("path")
  , log = require("./log.js")
  , find = require("./find.js")
  , asyncMap = require("slide").asyncMap
  , npm = require("../npm.js")
  , fs = require("graceful-fs")

function loadPackageDefaults (pkg, pkgDir, cb) {
  if (!pkg) return cb(new Error("no package to load defaults from!"))
  if (typeof pkgDir === "function") {
    cb = pkgDir
    pkgDir = path.join(npm.dir, pkg.name, pkg.version, "package")
  }
  if (!pkgDir) pkgDir = "."

  if (pkg._defaultsLoaded) return cb(null, pkg)

  pkg._defaultsLoaded = true
  asyncMap
    ( [pkg]
    , function (pkg, cb) { log.verbose(pkg._id, "loadDefaults", cb) }
    , readDefaultBins(pkgDir)
    , readDefaultMans(pkgDir)
    , function (er) { cb(er, pkg) } )
}

function objectForEach (obj, fn) {
  Object.keys(obj).forEach(function (k) {
    fn(obj[k])
  })
}

function readDefaultMans (pkgDir) { return function (pkg, cb) {
  var man = pkg.directories && pkg.directories.man
    , manDir = path.join(pkgDir, man)
  if (pkg.man && !Array.isArray(pkg.man)) pkg.man = [pkg.man]
  if (pkg.man || !man) return cb(null, pkg)
  find(manDir, /\.[0-9]+(\.gz)?$/, function (er, filenames) {
    if (er) return cb(er)
    var cut = pkgDir === "." ? 0 : pkgDir.length + 1
    pkg.man = (filenames || []).map(function (filename) {
      return filename.substr(cut)
    }).filter(function (f) {
      return !f.match(/(^|\/)\./)
    })
    cb(null,pkg)
  })
}}

function readDefaultBins (pkgDir) { return function (pkg, cb) {
  var bin = pkg.directories && pkg.directories.bin
  if (pkg.bins) pkg.bin = pkg.bins, delete pkg.bins
  if (pkg.bin || !bin) return cb(null, pkg)
  log.verbose("linking default bins", pkg._id)
  var binDir = path.join(pkgDir, bin)
  pkg.bin = {}
  find(binDir, function (er, filenames) {
    if (er || !filenames || !filenames.length) return cb(er, pkg)
    var cut = pkgDir === "." ? 0 : pkgDir.length + 1
      , binCut = pkgDir === "." ? bin.length - 1 : binDir.length + 1
    filenames.filter(function (f) {
      return !f.substr(binCut).match(/(^|\/)\./)
    }).forEach(function (filename) {
      var key = filename.substr(binCut)
                        .replace(/\.(js|node)$/, '')
        , val = filename.substr(cut)
      if (key.length && val.length) pkg.bin[key] = val
    })
    log.silly(pkg.bin, pkg._id+".bin")
    cb(null, pkg)
  })
}}
