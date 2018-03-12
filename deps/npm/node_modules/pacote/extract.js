'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const extractStream = require('./lib/extract-stream')
const fs = require('fs')
const mkdirp = BB.promisify(require('mkdirp'))
const npa = require('npm-package-arg')
const optCheck = require('./lib/util/opt-check')
const path = require('path')
const retry = require('promise-retry')
const rimraf = BB.promisify(require('rimraf'))

const truncateAsync = BB.promisify(fs.truncate)
const readFileAsync = BB.promisify(fs.readFile)
const appendFileAsync = BB.promisify(fs.appendFile)

module.exports = extract
function extract (spec, dest, opts) {
  opts = optCheck(opts)
  spec = npa(spec, opts.where)
  const startTime = Date.now()
  if (opts.integrity && opts.cache && !opts.preferOnline) {
    opts.log.silly('pacote', `trying ${spec} by hash: ${opts.integrity}`)
    return extractByDigest(
      startTime, spec, dest, opts
    ).catch(err => {
      if (err.code === 'ENOENT') {
        opts.log.silly('pacote', `data for ${opts.integrity} not present. Using manifest.`)
        return extractByManifest(startTime, spec, dest, opts)
      }

      if (err.code === 'EINTEGRITY' || err.code === 'Z_DATA_ERROR') {
        opts.log.warn('pacote', `cached data for ${spec} (${opts.integrity}) seems to be corrupted. Refreshing cache.`)
      }
      return cleanUpCached(
        dest, opts.cache, opts.integrity, opts
      ).then(() => {
        return extractByManifest(startTime, spec, dest, opts)
      })
    })
  } else {
    opts.log.silly('pacote', 'no tarball hash provided for', spec.name, '- extracting by manifest')
    return BB.resolve(retry((tryAgain, attemptNum) => {
      return extractByManifest(
        startTime, spec, dest, opts
      ).catch(err => {
        // Retry once if we have a cache, to clear up any weird conditions.
        // Don't retry network errors, though -- make-fetch-happen has already
        // taken care of making sure we're all set on that front.
        if (opts.cache && !err.code.match(/^E\d{3}$/)) {
          if (err.code === 'EINTEGRITY' || err.code === 'Z_DATA_ERROR') {
            opts.log.warn('pacote', `tarball data for ${spec} (${opts.integrity}) seems to be corrupted. Trying one more time.`)
          }
          return cleanUpCached(
            dest, opts.cache, err.sri, opts
          ).then(() => tryAgain(err))
        } else {
          throw err
        }
      })
    }, {retries: 1}))
  }
}

function extractByDigest (start, spec, dest, opts) {
  return mkdirp(dest).then(() => {
    const xtractor = extractStream(spec, dest, opts)
    const cached = cacache.get.stream.byDigest(opts.cache, opts.integrity, opts)
    cached.pipe(xtractor)
    return new BB((resolve, reject) => {
      cached.on('error', reject)
      xtractor.on('error', reject)
      xtractor.on('close', resolve)
    })
  }).then(() => {
    opts.log.silly('pacote', `${spec} extracted to ${dest} by content address ${Date.now() - start}ms`)
  }).catch(err => {
    if (err.code === 'EINTEGRITY') {
      err.message = `Verification failed while extracting ${spec}:\n${err.message}`
    }
    throw err
  })
}

let fetch
function extractByManifest (start, spec, dest, opts) {
  let integrity = opts.integrity
  let resolved = opts.resolved
  return mkdirp(dest).then(() => {
    const xtractor = extractStream(spec, dest, opts)
    if (!fetch) {
      fetch = require('./lib/fetch')
    }
    const tardata = fetch.tarball(spec, opts)
    if (!resolved) {
      tardata.on('manifest', m => {
        resolved = m._resolved
      })
      tardata.on('integrity', i => {
        integrity = i
      })
    }
    tardata.pipe(xtractor)
    return new BB((resolve, reject) => {
      tardata.on('error', reject)
      xtractor.on('error', reject)
      xtractor.on('close', resolve)
    })
  }).then(() => {
    if (!opts.resolved) {
      const pjson = path.join(dest, 'package.json')
      return readFileAsync(pjson, 'utf8')
      .then(str => {
        return truncateAsync(pjson)
        .then(() => {
          return appendFileAsync(pjson, str.replace(
            /}\s*$/,
            `\n,"_resolved": ${
              JSON.stringify(resolved || '')
            }\n,"_integrity": ${
              JSON.stringify(integrity || '')
            }\n,"_from": ${
              JSON.stringify(spec.toString())
            }\n}`
          ))
        })
      })
    }
  }).then(() => {
    opts.log.silly('pacote', `${spec} extracted in ${Date.now() - start}ms`)
  }).catch(err => {
    if (err.code === 'EINTEGRITY') {
      err.message = `Verification failed while extracting ${spec}:\n${err.message}`
    }
    throw err
  })
}

function cleanUpCached (dest, cachePath, integrity, opts) {
  return BB.join(
    rimraf(dest),
    cacache.rm.content(cachePath, integrity, opts)
  )
}
