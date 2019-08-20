const npm = require('../npm.js')
const path = require('path')
const chownr = require('chownr')
const writeFileAtomic = require('write-file-atomic')
const mkdirp = require('mkdirp')
const fs = require('graceful-fs')

let cache = null
let cacheUid = null
let cacheGid = null
let needChown = typeof process.getuid === 'function'

const getCacheOwner = () => {
  let st
  try {
    st = fs.lstatSync(cache)
  } catch (er) {
    if (er.code !== 'ENOENT') {
      throw er
    }
    st = fs.lstatSync(path.dirname(cache))
  }

  cacheUid = st.uid
  cacheGid = st.gid

  needChown = st.uid !== process.getuid() ||
    st.gid !== process.getgid()
}

const writeOrAppend = (method, file, data) => {
  if (!cache) {
    cache = npm.config.get('cache')
  }

  // redundant if already absolute, but prevents non-absolute files
  // from being written as if they're part of the cache.
  file = path.resolve(cache, file)

  if (cacheUid === null && needChown) {
    getCacheOwner()
  }

  const dir = path.dirname(file)
  const firstMade = mkdirp.sync(dir)

  if (!needChown) {
    return method(file, data)
  }

  let methodThrew = true
  try {
    method(file, data)
    methodThrew = false
  } finally {
    // always try to leave it in the right ownership state, even on failure
    // let the method error fail it instead of the chownr error, though
    if (!methodThrew) {
      chownr.sync(firstMade || file, cacheUid, cacheGid)
    } else {
      try {
        chownr.sync(firstMade || file, cacheUid, cacheGid)
      } catch (_) {}
    }
  }
}

exports.append = (file, data) => writeOrAppend(fs.appendFileSync, file, data)
exports.write = (file, data) => writeOrAppend(writeFileAtomic.sync, file, data)
