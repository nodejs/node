'use strict'

const BB = require('bluebird')

const fs = BB.promisifyAll(require('graceful-fs'))
const gentlyRm = BB.promisify(require('../../utils/gently-rm.js'))
const log = require('npmlog')
const mkdirp = BB.promisify(require('mkdirp'))
const moduleName = require('../../utils/module-name.js')
const moduleStagingPath = require('../module-staging-path.js')
const move = BB.promisify(require('../../utils/move.js'))
const npa = require('npm-package-arg')
const packageId = require('../../utils/package-id.js')
const pacote = require('pacote')
let pacoteOpts
const path = require('path')

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
  return pacote.extract(
    pkg.package._resolved
    ? npa.resolve(pkg.package.name, pkg.package._resolved)
    : pkg.package._requested,
    extractTo,
    opts
  ).then(() => {
    if (pkg.package.bundleDependencies) {
      return readBundled(pkg, staging, extractTo)
    }
  }).then(() => {
    return gentlyRm(path.join(extractTo, 'node_modules'))
  })
}

function readBundled (pkg, staging, extractTo) {
  return BB.map(pkg.children, (child) => {
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
    return fs.statAsync(stageFrom).then(() => {
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
