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
      const parsed = parsePkgLock(shrinkwrap || lockfile, name)
      if (parsed && parsed.lockfileVersion !== PKGLOCK_VERSION) {
        log.warn('read-shrinkwrap', `This version of npm is compatible with lockfileVersion@${PKGLOCK_VERSION}, but ${name} was generated for lockfileVersion@${parsed.lockfileVersion || 0}. I'll try to do my best with it!`)
      }
      child.package._shrinkwrap = parsed
    }
  ).then(() => next(), next)
}

function maybeReadFile (name, child) {
  return readFileAsync(
    path.join(child.path, name),
    'utf8'
  ).catch({code: 'ENOENT'}, () => null)
}

module.exports.andInflate = function (child, next) {
  readShrinkwrap(child, iferr(next, function () {
    if (child.package._shrinkwrap) {
      return inflateShrinkwrap(child, child.package._shrinkwrap || {}, next)
    } else {
      return next()
    }
  }))
}

const PARENT_RE = /\|{7,}/g
const OURS_RE = /<{7,}/g
const THEIRS_RE = /={7,}/g
const END_RE = />{7,}/g

module.exports._isDiff = isDiff
function isDiff (str) {
  return str.match(OURS_RE) && str.match(THEIRS_RE) && str.match(END_RE)
}

module.exports._parsePkgLock = parsePkgLock
function parsePkgLock (str, filename) {
  if (!str) { return null }
  try {
    return parseJSON(str)
  } catch (e) {
    if (isDiff(str)) {
      log.warn('conflict', `A git conflict was detected in ${filename}. Attempting to auto-resolve.`)
      log.warn('conflict', 'To make this happen automatically on git rebase/merge, consider using the npm-merge-driver:')
      log.warn('conflict', '$ npx npm-merge-driver install -g')
      const pieces = str.split(/[\n\r]+/g).reduce((acc, line) => {
        if (line.match(PARENT_RE)) acc.state = 'parent'
        else if (line.match(OURS_RE)) acc.state = 'ours'
        else if (line.match(THEIRS_RE)) acc.state = 'theirs'
        else if (line.match(END_RE)) acc.state = 'top'
        else {
          if (acc.state === 'top' || acc.state === 'ours') acc.ours += line
          if (acc.state === 'top' || acc.state === 'theirs') acc.theirs += line
          if (acc.state === 'top' || acc.state === 'parent') acc.parent += line
        }
        return acc
      }, {
        state: 'top',
        ours: '',
        theirs: '',
        parent: ''
      })
      try {
        const ours = parseJSON(pieces.ours)
        const theirs = parseJSON(pieces.theirs)
        return reconcileLockfiles(ours, theirs)
      } catch (_e) {
        log.error('conflict', `Automatic conflict resolution failed. Please manually resolve conflicts in ${filename} and try again.`)
        log.silly('conflict', `Error during resolution: ${_e}`)
        throw e
      }
    } else {
      throw e
    }
  }
}

function reconcileLockfiles (parent, ours, theirs) {
  return Object.assign({}, ours, theirs)
}
