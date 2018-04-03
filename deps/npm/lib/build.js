// npm build command

// everything about the installation after the creation of
// the .npm/{name}/{version}/package folder.
// linking the modules into the npm.root,
// resolving dependencies, etc.

// This runs AFTER install or link are completed.

var npm = require('./npm.js')
var log = require('npmlog')
var chain = require('slide').chain
var path = require('path')
var fs = require('graceful-fs')
var lifecycle = require('./utils/lifecycle.js')
var readJson = require('read-package-json')
var binLinks = require('bin-links')
var binLinksConfig = require('./config/bin-links.js')
var ini = require('ini')
var writeFile = require('write-file-atomic')

module.exports = build
build.usage = 'npm build [<folder>]'

build._didBuild = {}
build._noLC = {}
function build (args, global, didPre, didRB, cb) {
  if (typeof cb !== 'function') {
    cb = didRB
    didRB = false
  }
  if (typeof cb !== 'function') {
    cb = didPre
    didPre = false
  }
  if (typeof cb !== 'function') {
    cb = global
    global = npm.config.get('global')
  }

  if (!args.length) {
    readJson(path.resolve(npm.localPrefix, 'package.json'), function (er, pkg) {
      if (!args.length && pkg && pkg.scripts && pkg.scripts.build) {
        log.warn('build', '`npm build` called with no arguments. Did you mean to `npm run-script build`?')
      }
      cb()
    })
  } else {
    // it'd be nice to asyncMap these, but actually, doing them
    // in parallel generally munges up the output from node-waf
    var builder = build_(global, didPre, didRB)
    chain(args.map(function (arg) {
      return function (cb) {
        builder(arg, cb)
      }
    }), cb)
  }
}

function build_ (global, didPre, didRB) {
  return function (folder, cb) {
    folder = path.resolve(folder)
    if (build._didBuild[folder]) log.info('build', 'already built', folder)
    build._didBuild[folder] = true
    log.info('build', folder)
    readJson(path.resolve(folder, 'package.json'), function (er, pkg) {
      if (er) return cb(er)
      chain([
        !didPre && [lifecycle, pkg, 'preinstall', folder],
        [linkStuff, pkg, folder, global],
        !didRB && [rebuildBundles, pkg, folder],
        [writeBuiltinConf, pkg, folder],
        didPre !== build._noLC && [lifecycle, pkg, 'install', folder],
        didPre !== build._noLC && [lifecycle, pkg, 'postinstall', folder]
      ],
      cb)
    })
  }
}

var writeBuiltinConf = build.writeBuiltinConf = function (pkg, folder, cb) {
  // the builtin config is "sticky". Any time npm installs
  // itself globally, it puts its builtin config file there
  var parent = path.dirname(folder)
  var dir = npm.globalDir

  // Make this count for canary, too
  if ((pkg.name !== 'npm' && pkg.name !== 'npmc') ||
      !npm.config.get('global') ||
      !npm.config.usingBuiltin ||
      dir !== parent) {
    return cb()
  }

  var data = ini.stringify(npm.config.sources.builtin.data)
  writeFile(path.resolve(folder, 'npmrc'), data, cb)
}

var linkStuff = build.linkStuff = function (pkg, folder, global, cb) {
  // allow to opt out of linking binaries.
  if (npm.config.get('bin-links') === false) return cb()
  return binLinks(pkg, folder, global, binLinksConfig(pkg), cb)
}

function rebuildBundles (pkg, folder, cb) {
  if (!npm.config.get('rebuild-bundle')) return cb()

  var deps = Object.keys(pkg.dependencies || {})
             .concat(Object.keys(pkg.devDependencies || {}))
  var bundles = pkg.bundleDependencies || pkg.bundledDependencies || []

  fs.readdir(path.resolve(folder, 'node_modules'), function (er, files) {
    // error means no bundles
    if (er) return cb()

    log.verbose('rebuildBundles', files)
    // don't asyncMap these, because otherwise build script output
    // gets interleaved and is impossible to read
    chain(files.filter(function (file) {
      // rebuild if:
      // not a .folder, like .bin or .hooks
      return !file.match(/^[\._-]/) &&
          // not some old 0.x style bundle
          file.indexOf('@') === -1 &&
          // either not a dep, or explicitly bundled
          (deps.indexOf(file) === -1 || bundles.indexOf(file) !== -1)
    }).map(function (file) {
      file = path.resolve(folder, 'node_modules', file)
      return function (cb) {
        if (build._didBuild[file]) return cb()
        log.verbose('rebuild bundle', file)
        // if file is not a package dir, then don't do it.
        fs.lstat(path.resolve(file, 'package.json'), function (er) {
          if (er) return cb()
          build_(false)(file, cb)
        })
      }
    }), cb)
  })
}
