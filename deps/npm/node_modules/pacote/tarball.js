'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const fs = require('fs')
const getStream = require('get-stream')
const mkdirp = BB.promisify(require('mkdirp'))
const npa = require('npm-package-arg')
const optCheck = require('./lib/util/opt-check')
const path = require('path')
const pipe = BB.promisify(require('mississippi').pipe)

module.exports = tarball
function tarball (spec, opts) {
  opts = optCheck(opts)
  spec = npa(spec, opts.where)
  const startTime = Date.now()
  if (opts.integrity && !opts.preferOnline) {
    opts.log.silly('tarball', 'checking if', opts.integrity, 'is already cached')
    return cacache.get.byDigest(opts.cache, opts.integrity).then(data => {
      if (data) {
        opts.log.silly('tarball', `cached content available for ${spec} (${Date.now() - startTime}ms)`)
        return data
      } else {
        return getStream.buffer(tarballByManifest(startTime, spec, opts))
      }
    })
  } else {
    opts.log.silly('tarball', `no integrity hash provided for ${spec} - fetching by manifest`)
    return getStream.buffer(tarballByManifest(startTime, spec, opts))
  }
}

module.exports.stream = tarballStream
function tarballStream (spec, opts) {
  opts = optCheck(opts)
  spec = npa(spec, opts.where)
  const startTime = Date.now()
  if (opts.integrity && !opts.preferOnline) {
    opts.log.silly('tarball', 'checking if', opts.integrity, 'is already cached')
    return cacache.get.hasContent(opts.cache, opts.integrity).then(info => {
      if (info) {
        opts.log.silly('tarball', `cached content available for ${spec} (${Date.now() - startTime}ms)`)
        return cacache.get.stream.byDigest(opts.cache, opts.integrity, opts)
      } else {
        return tarballByManifest(startTime, spec, opts)
      }
    })
  } else {
    opts.log.silly('tarball', `no integrity hash provided for ${spec} - fetching by manifest`)
    return tarballByManifest(startTime, spec, opts)
  }
}

module.exports.toFile = tarballToFile
function tarballToFile (spec, dest, opts) {
  opts = optCheck(opts)
  spec = npa(spec, opts.where)
  const startTime = Date.now()
  return mkdirp(path.dirname(dest)).then(() => {
    if (opts.integrity && !opts.preferOnline) {
      opts.log.silly('tarball', 'checking if', opts.integrity, 'is already cached')
      return cacache.get.copy.byDigest(opts.cache, opts.integrity, dest, opts)
      .then(() => {
        opts.log.silly('tarball', `cached content for ${spec} copied (${Date.now() - startTime}ms)`)
      }, err => {
        if (err.code === 'ENOENT') {
          return pipe(
            tarballByManifest(startTime, spec, opts),
            fs.createWriteStream(dest)
          )
        } else {
          throw err
        }
      })
    } else {
      opts.log.silly('tarball', `no integrity hash provided for ${spec} - fetching by manifest`)
      return pipe(
        tarballByManifest(startTime, spec, opts),
        fs.createWriteStream(dest)
      )
    }
  })
}

let fetch
function tarballByManifest (start, spec, opts) {
  if (!fetch) {
    fetch = require('./lib/fetch')
  }
  return fetch.tarball(spec, opts).on('end', () => {
    opts.log.silly('tarball', `${spec} done in ${Date.now() - start}ms`)
  })
}
