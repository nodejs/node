'use strict'

const BB = require('bluebird')

const stat = BB.promisify(require('graceful-fs').stat)
const gentlyRm = BB.promisify(require('../../utils/gently-rm.js'))
const log = require('npmlog')
const mkdirp = BB.promisify(require('mkdirp'))
const moduleName = require('../../utils/module-name.js')
const moduleStagingPath = require('../module-staging-path.js')
const move = require('../../utils/move.js')
const npa = require('npm-package-arg')
const npm = require('../../npm.js')
const packageId = require('../../utils/package-id.js')
let pacoteOpts
const path = require('path')
const localWorker = require('./extract-worker.js')
const workerFarm = require('worker-farm')

const WORKER_PATH = require.resolve('./extract-worker.js')
let workers

// NOTE: temporarily disabled on non-OSX due to ongoing issues:
//
// * Seems to make Windows antivirus issues much more common
// * Messes with Docker (I think)
//
// There are other issues that should be fixed that affect OSX too:
//
// * Logging is messed up right now because pacote does its own thing
// * Global deduplication in pacote breaks due to multiple procs
//
// As these get fixed, we can start experimenting with re-enabling it
// at least on some platforms.
const ENABLE_WORKERS = process.platform === 'darwin'

extract.init = () => {
  if (ENABLE_WORKERS) {
    workers = workerFarm({
      maxConcurrentCallsPerWorker: npm.limit.fetch,
      maxRetries: 1
    }, WORKER_PATH)
  }
  return BB.resolve()
}
extract.teardown = () => {
  if (ENABLE_WORKERS) {
    workerFarm.end(workers)
    workers = null
  }
  return BB.resolve()
}
module.exports = extract
function extract (staging, pkg, log) {
  log.silly('extract', packageId(pkg))
  const extractTo = moduleStagingPath(staging, pkg)
  if (!pacoteOpts) {
    pacoteOpts = require('../../config/pacote')
  }
  const opts = pacoteOpts({
    integrity: pkg.package._integrity
  })
  const args = [
    pkg.package._resolved
    ? npa.resolve(pkg.package.name, pkg.package._resolved)
    : pkg.package._requested,
    extractTo,
    opts
  ]
  return BB.fromNode((cb) => {
    let launcher = localWorker
    let msg = args
    const spec = typeof args[0] === 'string' ? npa(args[0]) : args[0]
    args[0] = spec.raw
    if (ENABLE_WORKERS && (spec.registry || spec.type === 'remote')) {
      // We can't serialize these options
      opts.loglevel = opts.log.level
      opts.log = null
      opts.dirPacker = null
      // workers will run things in parallel!
      launcher = workers
      try {
        msg = JSON.stringify(msg)
      } catch (e) {
        return cb(e)
      }
    }
    launcher(msg, cb)
  }).then(() => {
    if (pkg.package.bundleDependencies || anyBundled(pkg)) {
      return readBundled(pkg, staging, extractTo)
    }
  }).then(() => {
    return gentlyRm(path.join(extractTo, 'node_modules'))
  })
}

function anyBundled (top, pkg) {
  if (!pkg) pkg = top
  return pkg.children.some((child) => child.fromBundle === top || anyBundled(top, child))
}

function readBundled (pkg, staging, extractTo) {
  return BB.map(pkg.children, (child) => {
    if (!child.fromBundle) return
    if (child.error) {
      throw child.error
    } else {
      return stageBundledModule(pkg, child, staging, extractTo)
    }
  }, {concurrency: 10})
}

function getTree (pkg) {
  while (pkg.parent) pkg = pkg.parent
  return pkg
}

function warn (pkg, code, msg) {
  const tree = getTree(pkg)
  const err = new Error(msg)
  err.code = code
  tree.warnings.push(err)
}

function stageBundledModule (bundler, child, staging, parentPath) {
  const stageFrom = path.join(parentPath, 'node_modules', child.package.name)
  const stageTo = moduleStagingPath(staging, child)

  return BB.map(child.children, (child) => {
    if (child.error) {
      throw child.error
    } else {
      return stageBundledModule(bundler, child, staging, stageFrom)
    }
  }).then(() => {
    return finishModule(bundler, child, stageTo, stageFrom)
  })
}

function finishModule (bundler, child, stageTo, stageFrom) {
  // If we were the one's who bundled this module…
  if (child.fromBundle === bundler) {
    return mkdirp(path.dirname(stageTo)).then(() => {
      return move(stageFrom, stageTo)
    })
  } else {
    return stat(stageFrom).then(() => {
      const bundlerId = packageId(bundler)
      if (!getTree(bundler).warnings.some((w) => {
        return w.code === 'EBUNDLEOVERRIDE'
      })) {
        warn(bundler, 'EBUNDLEOVERRIDE', `${bundlerId} had bundled packages that do not match the required version(s). They have been replaced with non-bundled versions.`)
      }
      log.verbose('bundle', `EBUNDLEOVERRIDE: Replacing ${bundlerId}'s bundled version of ${moduleName(child)} with ${packageId(child)}.`)
      return gentlyRm(stageFrom)
    }, () => {})
  }
}
