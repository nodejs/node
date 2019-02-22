'use strict'

const deprCheck = require('./utils/depr-check')
const path = require('path')
const log = require('npmlog')
const readPackageTree = require('read-package-tree')
const rimraf = require('rimraf')
const validate = require('aproba')
const npa = require('npm-package-arg')
const npm = require('./npm')
let npmConfig
const npmlog = require('npmlog')
const limit = require('call-limit')
const tempFilename = require('./utils/temp-filename')
const pacote = require('pacote')
const isWindows = require('./utils/is-windows.js')

function andLogAndFinish (spec, tracker, done) {
  validate('SOF|SZF|OOF|OZF', [spec, tracker, done])
  return (er, pkg) => {
    if (er) {
      log.silly('fetchPackageMetaData', 'error for ' + String(spec), er.message)
      if (tracker) tracker.finish()
    }
    return done(er, pkg)
  }
}

const CACHE = require('lru-cache')({
  max: 300 * 1024 * 1024,
  length: (p) => p._contentLength
})

module.exports = limit(fetchPackageMetadata, npm.limit.fetch)
function fetchPackageMetadata (spec, where, opts, done) {
  validate('SSOF|SSFZ|OSOF|OSFZ', [spec, where, opts, done])

  if (!done) {
    done = opts
    opts = {}
  }
  var tracker = opts.tracker
  const logAndFinish = andLogAndFinish(spec, tracker, done)

  if (typeof spec === 'object') {
    var dep = spec
  } else {
    dep = npa(spec)
  }
  if (!isWindows && dep.type === 'directory' && /^[a-zA-Z]:/.test(dep.fetchSpec)) {
    var err = new Error(`Can't install from windows path on a non-windows system: ${dep.fetchSpec.replace(/[/]/g, '\\')}`)
    err.code = 'EWINDOWSPATH'
    return logAndFinish(err)
  }
  if (!npmConfig) {
    npmConfig = require('./config/figgy-config.js')
  }
  pacote.manifest(dep, npmConfig({
    annotate: true,
    fullMetadata: opts.fullMetadata,
    log: tracker || npmlog,
    memoize: CACHE,
    where: where
  })).then(
    (pkg) => logAndFinish(null, deprCheck(pkg)),
    (err) => {
      if (dep.type !== 'directory') return logAndFinish(err)
      if (err.code === 'ENOTDIR') {
        var enolocal = new Error(`Could not install "${path.relative(process.cwd(), dep.fetchSpec)}" as it is not a directory and is not a file with a name ending in .tgz, .tar.gz or .tar`)
        enolocal.code = 'ENOLOCAL'
        if (err.stack) enolocal.stack = err.stack
        return logAndFinish(enolocal)
      } else if (err.code === 'ENOPACKAGEJSON') {
        var enopackage = new Error(`Could not install from "${path.relative(process.cwd(), dep.fetchSpec)}" as it does not contain a package.json file.`)
        enopackage.code = 'ENOLOCAL'
        if (err.stack) enopackage.stack = err.stack
        return logAndFinish(enopackage)
      } else {
        return logAndFinish(err)
      }
    }
  )
}

module.exports.addBundled = addBundled
function addBundled (pkg, next) {
  validate('OF', arguments)
  if (pkg._bundled !== undefined) return next(null, pkg)

  if (!pkg.bundleDependencies && pkg._requested.type !== 'directory') return next(null, pkg)
  const requested = pkg._requested || npa(pkg._from)
  if (requested.type === 'directory') {
    pkg._bundled = null
    return readPackageTree(pkg._requested.fetchSpec, function (er, tree) {
      if (tree) pkg._bundled = tree.children
      return next(null, pkg)
    })
  }
  pkg._bundled = null
  const target = tempFilename('unpack')
  if (!npmConfig) {
    npmConfig = require('./config/figgy-config.js')
  }
  const opts = npmConfig({integrity: pkg._integrity})
  pacote.extract(pkg._resolved || pkg._requested || npa.resolve(pkg.name, pkg.version), target, opts).then(() => {
    log.silly('addBundled', 'read tarball')
    readPackageTree(target, (err, tree) => {
      if (err) { return next(err) }
      log.silly('cleanup', 'remove extracted module')
      rimraf(target, function () {
        if (tree) {
          pkg._bundled = tree.children
        }
        next(null, pkg)
      })
    })
  }, next)
}
