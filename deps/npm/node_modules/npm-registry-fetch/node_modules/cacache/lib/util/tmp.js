'use strict'

const BB = require('bluebird')

const fixOwner = require('./fix-owner')
const path = require('path')
const rimraf = BB.promisify(require('rimraf'))
const uniqueFilename = require('unique-filename')

module.exports.mkdir = mktmpdir
function mktmpdir (cache, opts) {
  opts = opts || {}
  const tmpTarget = uniqueFilename(path.join(cache, 'tmp'), opts.tmpPrefix)
  return fixOwner.mkdirfix(tmpTarget, opts.uid, opts.gid).then(() => {
    return tmpTarget
  })
}

module.exports.withTmp = withTmp
function withTmp (cache, opts, cb) {
  if (!cb) {
    cb = opts
    opts = null
  }
  opts = opts || {}
  return BB.using(mktmpdir(cache, opts).disposer(rimraf), cb)
}

module.exports.fix = fixtmpdir
function fixtmpdir (cache, opts) {
  return fixOwner(path.join(cache, 'tmp'), opts.uid, opts.gid)
}
