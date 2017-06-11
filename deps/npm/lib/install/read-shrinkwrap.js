'use strict'

const BB = require('bluebird')

const fs = require('graceful-fs')
const iferr = require('iferr')
const inflateShrinkwrap = require('./inflate-shrinkwrap.js')
const log = require('npmlog')
const parseJSON = require('../utils/parse-json.js')
const path = require('path')
const PKGLOCK_VERSION = require('../npm.js').lockfileVersion

const readFileAsync = BB.promisify(fs.readFile)

module.exports = readShrinkwrap
function readShrinkwrap (child, next) {
  if (child.package._shrinkwrap) return process.nextTick(next)
  BB.join(
    maybeReadFile('npm-shrinkwrap.json', child),
    // Don't read non-root lockfiles
    child.isTop && maybeReadFile('package-lock.json', child),
    child.isTop && maybeReadFile('package.json', child),
    (shrinkwrap, lockfile, pkgJson) => {
      if (shrinkwrap && lockfile) {
        log.warn('read-shrinkwrap', 'Ignoring package-lock.json because there is already an npm-shrinkwrap.json. Please use only one of the two.')
      }
      const name = shrinkwrap ? 'npm-shrinkwrap.json' : 'package-lock.json'
      let parsed = null
      if (shrinkwrap || lockfile) {
        try {
          parsed = parseJSON(shrinkwrap || lockfile)
        } catch (ex) {
          throw ex
        }
      }
      if (parsed && parsed.lockfileVersion !== PKGLOCK_VERSION) {
        log.warn('read-shrinkwrap', `This version of npm is compatible with lockfileVersion@${PKGLOCK_VERSION}, but ${name} was generated for lockfileVersion@${parsed.lockfileVersion || 0}. I'll try to do my best with it!`)
      }
      child.package._shrinkwrap = parsed
    }
  ).then(() => next(), next)
}

function maybeReadFile (name, child) {
  return readFileAsync(
    path.join(child.path, name)
  ).catch({code: 'ENOENT'}, () => null)
}

module.exports.andInflate = function (child, opts, next) {
  if (arguments.length === 2) {
    next = opts
    opts = {}
  }
  readShrinkwrap(child, iferr(next, function () {
    if (child.package._shrinkwrap) {
      return inflateShrinkwrap(child, child.package._shrinkwrap.dependencies || {}, opts, next)
    } else {
      return next()
    }
  }))
}
