'use strict'

const BB = require('bluebird')

const contentPath = require('./path')
const fs = require('graceful-fs')
const PassThrough = require('stream').PassThrough
const pipe = BB.promisify(require('mississippi').pipe)
const ssri = require('ssri')
const Y = require('../util/y.js')

BB.promisifyAll(fs)

module.exports = read
function read (cache, integrity, opts) {
  opts = opts || {}
  return pickContentSri(cache, integrity).then(content => {
    const sri = content.sri
    const cpath = contentPath(cache, sri)
    return fs.readFileAsync(cpath, null).then(data => {
      if (typeof opts.size === 'number' && opts.size !== data.length) {
        throw sizeError(opts.size, data.length)
      } else if (ssri.checkData(data, sri)) {
        return data
      } else {
        throw integrityError(sri, cpath)
      }
    })
  })
}

module.exports.stream = readStream
module.exports.readStream = readStream
function readStream (cache, integrity, opts) {
  opts = opts || {}
  const stream = new PassThrough()
  pickContentSri(
    cache, integrity
  ).then(content => {
    const sri = content.sri
    return pipe(
      fs.createReadStream(contentPath(cache, sri)),
      ssri.integrityStream({
        integrity: sri,
        size: opts.size
      }),
      stream
    )
  }).catch(err => {
    stream.emit('error', err)
  })
  return stream
}

if (fs.copyFile) {
  module.exports.copy = copy
}
function copy (cache, integrity, dest, opts) {
  opts = opts || {}
  return pickContentSri(cache, integrity).then(content => {
    const sri = content.sri
    const cpath = contentPath(cache, sri)
    return fs.copyFileAsync(cpath, dest).then(() => content.size)
  })
}

module.exports.hasContent = hasContent
function hasContent (cache, integrity) {
  if (!integrity) { return BB.resolve(false) }
  return pickContentSri(cache, integrity)
  .catch({code: 'ENOENT'}, () => false)
  .catch({code: 'EPERM'}, err => {
    if (process.platform !== 'win32') {
      throw err
    } else {
      return false
    }
  }).then(content => {
    if (!content.sri) return false
    return ({ sri: content.sri, size: content.stat.size })
  })
}

module.exports._pickContentSri = pickContentSri
function pickContentSri (cache, integrity) {
  const sri = ssri.parse(integrity)
  // If `integrity` has multiple entries, pick the first digest
  // with available local data.
  const algo = sri.pickAlgorithm()
  const digests = sri[algo]
  if (digests.length <= 1) {
    const cpath = contentPath(cache, digests[0])
    return fs.lstatAsync(cpath).then(stat => ({ sri: digests[0], stat }))
  } else {
    return BB.any(sri[sri.pickAlgorithm()].map(meta => {
      return pickContentSri(cache, meta)
    }))
  }
}

function sizeError (expected, found) {
  var err = new Error(Y`Bad data size: expected inserted data to be ${expected} bytes, but got ${found} instead`)
  err.expected = expected
  err.found = found
  err.code = 'EBADSIZE'
  return err
}

function integrityError (sri, path) {
  var err = new Error(Y`Integrity verification failed for ${sri} (${path})`)
  err.code = 'EINTEGRITY'
  err.sri = sri
  err.path = path
  return err
}
