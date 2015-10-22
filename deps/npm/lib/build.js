// npm build command

// everything about the installation after the creation of
// the .npm/{name}/{version}/package folder.
// linking the modules into the npm.root,
// resolving dependencies, etc.

// This runs AFTER install or link are completed.

var npm = require("./npm.js")
  , log = require("npmlog")
  , chain = require("slide").chain
  , fs = require("graceful-fs")
  , path = require("path")
  , lifecycle = require("./utils/lifecycle.js")
  , readJson = require("read-package-json")
  , link = require("./utils/link.js")
  , linkIfExists = link.ifExists
  , cmdShim = require("cmd-shim")
  , cmdShimIfExists = cmdShim.ifExists
  , asyncMap = require("slide").asyncMap
  , ini = require("ini")
  , writeFile = require("write-file-atomic")

module.exports = build
build.usage = "npm build <folder>\n(this is plumbing)"

build._didBuild = {}
build._noLC = {}
function build (args, global, didPre, didRB, cb) {
  if (typeof cb !== "function") cb = didRB, didRB = false
  if (typeof cb !== "function") cb = didPre, didPre = false
  if (typeof cb !== "function") {
    cb = global, global = npm.config.get("global")
  }
  // it'd be nice to asyncMap these, but actually, doing them
  // in parallel generally munges up the output from node-waf
  var builder = build_(global, didPre, didRB)
  chain(args.map(function (arg) { return function (cb) {
    builder(arg, cb)
  }}), cb)
}

function build_ (global, didPre, didRB) { return function (folder, cb) {
  folder = path.resolve(folder)
  if (build._didBuild[folder]) log.info("build", "already built", folder)
  build._didBuild[folder] = true
  log.info("build", folder)
  readJson(path.resolve(folder, "package.json"), function (er, pkg) {
    if (er) return cb(er)
    chain
      ( [ !didPre && [lifecycle, pkg, "preinstall", folder]
        , [linkStuff, pkg, folder, global, didRB]
        , [writeBuiltinConf, pkg, folder]
        , didPre !== build._noLC && [lifecycle, pkg, "install", folder]
        , didPre !== build._noLC && [lifecycle, pkg, "postinstall", folder]
        , didPre !== build._noLC
          && npm.config.get("npat")
          && [lifecycle, pkg, "test", folder] ]
      , cb )
  })
}}

function writeBuiltinConf (pkg, folder, cb) {
  // the builtin config is "sticky". Any time npm installs
  // itself globally, it puts its builtin config file there
  var parent = path.dirname(folder)
  var dir = npm.globalDir

  if (pkg.name !== "npm" ||
      !npm.config.get("global") ||
      !npm.config.usingBuiltin ||
      dir !== parent) {
    return cb()
  }

  var data = ini.stringify(npm.config.sources.builtin.data)
  writeFile(path.resolve(folder, "npmrc"), data, cb)
}

function linkStuff (pkg, folder, global, didRB, cb) {
  // allow to opt out of linking binaries.
  if (npm.config.get("bin-links") === false) return cb()

  // if it's global, and folder is in {prefix}/node_modules,
  // then bins are in {prefix}/bin
  // otherwise, then bins are in folder/../.bin
  var parent = pkg.name[0] === '@' ? path.dirname(path.dirname(folder)) : path.dirname(folder)
  var gnm = global && npm.globalDir
  var gtop = parent === gnm

  log.info('linkStuff', pkg._id)
  log.silly('linkStuff', pkg._id, 'has', parent, 'as its parent node_modules')
  if (global) log.silly('linkStuff', pkg._id, 'is part of a global install')
  if (gnm) log.silly('linkStuff', pkg._id, 'is installed into a global node_modules')
  if (gtop) log.silly('linkStuff', pkg._id, 'is installed into the top-level global node_modules')

  shouldWarn(pkg, folder, global, function () {
    asyncMap(
      [linkBins, linkMans, !didRB && rebuildBundles],
      function (fn, cb) {
        if (!fn) return cb()
        log.verbose(fn.name, pkg._id)
        fn(pkg, folder, parent, gtop, cb)
      },
      cb
    )
  })
}

function shouldWarn(pkg, folder, global, cb) {
  var parent = path.dirname(folder)
    , top = parent === npm.dir
    , cwd = npm.localPrefix

  readJson(path.resolve(cwd, "package.json"), function(er, topPkg) {
    if (er) return cb(er)

    var linkedPkg = path.basename(cwd)
      , currentPkg = path.basename(folder)

    // current searched package is the linked package on first call
    if (linkedPkg !== currentPkg) {

      // don't generate a warning if it's listed in dependencies
      if (Object.keys(topPkg.dependencies || {})
          .concat(Object.keys(topPkg.devDependencies || {}))
          .indexOf(currentPkg) === -1) {

        if (top && pkg.preferGlobal && !global) {
          log.warn("prefer global", pkg._id + " should be installed with -g")
        }
      }
    }

    cb()
  })
}

function rebuildBundles (pkg, folder, parent, gtop, cb) {
  if (!npm.config.get("rebuild-bundle")) return cb()

  var deps = Object.keys(pkg.dependencies || {})
             .concat(Object.keys(pkg.devDependencies || {}))
    , bundles = pkg.bundleDependencies || pkg.bundledDependencies || []

  fs.readdir(path.resolve(folder, "node_modules"), function (er, files) {
    // error means no bundles
    if (er) return cb()

    log.verbose("rebuildBundles", files)
    // don't asyncMap these, because otherwise build script output
    // gets interleaved and is impossible to read
    chain(files.filter(function (file) {
      // rebuild if:
      // not a .folder, like .bin or .hooks
      return !file.match(/^[\._-]/)
          // not some old 0.x style bundle
          && file.indexOf("@") === -1
          // either not a dep, or explicitly bundled
          && (deps.indexOf(file) === -1 || bundles.indexOf(file) !== -1)
    }).map(function (file) {
      file = path.resolve(folder, "node_modules", file)
      return function (cb) {
        if (build._didBuild[file]) return cb()
        log.verbose("rebuild bundle", file)
        // if file is not a package dir, then don't do it.
        fs.lstat(path.resolve(file, "package.json"), function (er) {
          if (er) return cb()
          build_(false)(file, cb)
        })
    }}), cb)
  })
}

function linkBins (pkg, folder, parent, gtop, cb) {
  if (!pkg.bin || !gtop && path.basename(parent) !== "node_modules") {
    return cb()
  }
  var binRoot = gtop ? npm.globalBin
                     : path.resolve(parent, ".bin")
  log.verbose("link bins", [pkg.bin, binRoot, gtop])

  asyncMap(Object.keys(pkg.bin), function (b, cb) {
    linkBin( path.resolve(folder, pkg.bin[b])
           , path.resolve(binRoot, b)
           , gtop && folder
           , function (er) {
      if (er) return cb(er)
      // bins should always be executable.
      // XXX skip chmod on windows?
      var src = path.resolve(folder, pkg.bin[b])
      fs.chmod(src, npm.modes.exec, function (er) {
        if (er && er.code === "ENOENT" && npm.config.get("ignore-scripts")) {
          return cb()
        }
        if (er || !gtop) return cb(er)
        var dest = path.resolve(binRoot, b)
          , out = npm.config.get("parseable")
                ? dest + "::" + src + ":BINFILE"
                : dest + " -> " + src
        console.log(out)
        cb()
      })
    })
  }, cb)
}

function linkBin (from, to, gently, cb) {
  if (process.platform !== "win32") {
    return linkIfExists(from, to, gently, cb)
  } else {
    return cmdShimIfExists(from, to, cb)
  }
}

function linkMans (pkg, folder, parent, gtop, cb) {
  if (!pkg.man || !gtop || process.platform === "win32") return cb()

  var manRoot = path.resolve(npm.config.get("prefix"), "share", "man")
  log.verbose("linkMans", "man files are", pkg.man, "in", manRoot)

  // make sure that the mans are unique.
  // otherwise, if there are dupes, it'll fail with EEXIST
  var set = pkg.man.reduce(function (acc, man) {
    acc[path.basename(man)] = man
    return acc
  }, {})
  pkg.man = pkg.man.filter(function (man) {
    return set[path.basename(man)] === man
  })

  asyncMap(pkg.man, function (man, cb) {
    if (typeof man !== "string") return cb()
    log.silly("linkMans", "preparing to link", man)
    var parseMan = man.match(/(.*\.([0-9]+)(\.gz)?)$/)
    if (!parseMan) {
      return cb(new Error(
        man+" is not a valid name for a man file.  " +
        "Man files must end with a number, " +
        "and optionally a .gz suffix if they are compressed."
      ))
    }

    var stem = parseMan[1]
    var sxn = parseMan[2]
    var bn = path.basename(stem)
    var manSrc = path.resolve(folder, man)
    var manDest = path.join(manRoot, "man" + sxn, bn)

    linkIfExists(manSrc, manDest, gtop && folder, cb)
  }, cb)
}
