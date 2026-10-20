'use strict'

const crypto = require('crypto')
const { withTempDir } = require('@npmcli/fs')
const fs = require('fs/promises')
const path = require('path')
const cacheDir = require('./cache-dir')

module.exports.mkdir = mktmpdir

module.exports.tmpName = function tmpName (cache, tmpPrefix) {
  const id = crypto.randomUUID()
  return path.join(cache, 'tmp', tmpPrefix ? `${tmpPrefix}-${id}` : id)
}

async function mktmpdir (cache, opts = {}) {
  const { tmpPrefix } = opts
  const tmpDir = path.join(cache, 'tmp')
  await cacheDir.mkdir(cache)
  await fs.mkdir(tmpDir, { recursive: true, owner: 'inherit' })
  // do not use path.join(), it drops the trailing / if tmpPrefix is unset
  const target = `${tmpDir}${path.sep}${tmpPrefix || ''}`
  return fs.mkdtemp(target, { owner: 'inherit' })
}

module.exports.withTmp = withTmp

function withTmp (cache, opts, cb) {
  if (!cb) {
    cb = opts
    opts = {}
  }
  return cacheDir.mkdir(cache).then(() =>
    withTempDir(path.join(cache, 'tmp'), cb, opts)
  )
}
