'use strict'

const BB = require('bluebird')

const figgyPudding = require('figgy-pudding')
const stat = BB.promisify(require('graceful-fs').stat)
const gentlyRm = BB.promisify(require('../../utils/gently-rm.js'))
const mkdirp = BB.promisify(require('mkdirp'))
const moduleStagingPath = require('../module-staging-path.js')
const move = require('../../utils/move.js')
const npa = require('npm-package-arg')
const npm = require('../../npm.js')
let npmConfig
const packageId = require('../../utils/package-id.js')
const path = require('path')
const localWorker = require('./extract-worker.js')
const workerFarm = require('worker-farm')
const isRegistry = require('../../utils/is-registry.js')

const WORKER_PATH = require.resolve('./extract-worker.js')
let workers

const ExtractOpts = figgyPudding({
  log: {}
}, { other () { return true } })

// Disabled for now. Re-enable someday. Just not today.
const ENABLE_WORKERS = false

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
  if (!npmConfig) {
    npmConfig = require('../../config/figgy-config.js')
  }
  let opts = ExtractOpts(npmConfig()).concat({
    integrity: pkg.package._integrity,
    resolved: pkg.package._resolved
  })
  const args = [
    pkg.package._requested,
    extractTo,
    opts
  ]
  return BB.fromNode((cb) => {
    let launcher = localWorker
    let msg = args
    const spec = typeof args[0] === 'string' ? npa(args[0]) : args[0]
    args[0] = spec.raw
    if (ENABLE_WORKERS && (isRegistry(spec) || spec.type === 'remote')) {
      // We can't serialize these options
      opts = opts.concat({
        loglevel: opts.log.level,
        log: null,
        dirPacker: null,
        Promise: null,
        _events: null,
        _eventsCount: null,
        list: null,
        sources: null,
        _maxListeners: null,
        root: null
      })
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
    return stat(stageFrom).then(() => gentlyRm(stageFrom), () => {})
  }
}
