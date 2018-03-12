'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const fs = require('fs')
const getStream = require('get-stream')
const mkdirp = BB.promisify(require('mkdirp'))
const npa = require('npm-package-arg')
const optCheck = require('./lib/util/opt-check')
const PassThrough = require('stream').PassThrough
const path = require('path')
const pipeline = require('mississippi').pipeline
const ssri = require('ssri')
const url = require('url')

const readFileAsync = BB.promisify(fs.readFile)
const statAsync = BB.promisify(fs.stat)

module.exports = tarball
function tarball (spec, opts) {
  opts = optCheck(opts)
  spec = npa(spec, opts.where)
  const startTime = Date.now()
  if (opts.integrity && !opts.preferOnline) {
    const resolved = (
      opts.resolved &&
      url.parse(opts.resolved).protocol === 'file:' &&
      opts.resolved.substr(5)
    )
    const tryFile = resolved
    ? readFileAsync(resolved).then(
      data => {
        if (ssri.checkData(data, opts.integrity)) {
          opts.log.silly('tarball', `using local file content for ${spec}, found at ${opts.resolved} (${Date.now() - startTime}ms)`)
          return data
        } else {
          opts.log.silly('tarball', `content for ${spec} found at ${opts.resolved} invalid.`)
        }
      },
      err => { if (err.code === 'ENOENT') { return null } else { throw err } }
    )
    : BB.resolve(false)

    return tryFile
    .then(data => {
      if (data) {
        return data
      } else {
        opts.log.silly('tarball', 'checking if', opts.integrity, 'is already cached')
        return cacache.get.byDigest(opts.cache, opts.integrity).then(data => {
          if (data) {
            opts.log.silly('tarball', `cached content available for ${spec} (${Date.now() - startTime}ms)`)
            return data
          } else {
            return getStream.buffer(tarballByManifest(startTime, spec, opts))
          }
        })
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
  const stream = new PassThrough()
  if (opts.integrity && !opts.preferOnline) {
    const resolved = (
      opts.resolved &&
      url.parse(opts.resolved).protocol === 'file:' &&
      opts.resolved.substr(5)
    )
    const hasFile = resolved
    ? statAsync(resolved).then(
      () => true,
      err => { if (err.code === 'ENOENT') { return false } else { throw err } }
    )
    : BB.resolve(false)

    hasFile
    .then(hasFile => {
      if (hasFile) {
        opts.log.silly('tarball', `using local file content for ${spec}, found at ${opts.resolved}`)
        return pipeline(
          fs.createReadStream(resolved),
          ssri.integrityStream(opts.integrity)
        )
      } else {
        opts.log.silly('tarball', 'checking if', opts.integrity, 'is already cached')
        return cacache.get.hasContent(opts.cache, opts.integrity)
        .then(info => {
          if (info) {
            opts.log.silly('tarball', `cached content available for ${spec} (${Date.now() - startTime}ms)`)
            return cacache.get.stream.byDigest(opts.cache, opts.integrity, opts)
          } else {
            return tarballByManifest(startTime, spec, opts)
          }
        })
      }
    })
    .then(
      tarStream => tarStream
      .on('error', err => stream.emit('error', err))
      .pipe(stream),
      err => stream.emit('error', err)
    )
  } else {
    opts.log.silly('tarball', `no integrity hash provided for ${spec} - fetching by manifest`)
    tarballByManifest(startTime, spec, opts)
    .on('error', err => stream.emit('error', err))
    .pipe(stream)
  }
  return stream
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
          return new BB((resolve, reject) => {
            const tardata = tarballByManifest(startTime, spec, opts)
            const writer = fs.createWriteStream(dest)
            tardata.on('error', reject)
            writer.on('error', reject)
            writer.on('close', resolve)
            tardata.pipe(writer)
          })
        } else {
          throw err
        }
      })
    } else {
      opts.log.silly('tarball', `no integrity hash provided for ${spec} - fetching by manifest`)
      return new BB((resolve, reject) => {
        const tardata = tarballByManifest(startTime, spec, opts)
        const writer = fs.createWriteStream(dest)
        tardata.on('error', reject)
        writer.on('error', reject)
        writer.on('close', resolve)
        tardata.pipe(writer)
      })
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
