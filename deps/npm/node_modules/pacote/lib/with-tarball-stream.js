'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const fetch = require('./fetch.js')
const fs = require('fs')
const npa = require('npm-package-arg')
const optCheck = require('./util/opt-check.js')
const path = require('path')
const ssri = require('ssri')
const retry = require('promise-retry')

const statAsync = BB.promisify(fs.stat)

const RETRIABLE_ERRORS = new Set(['ENOENT', 'EINTEGRITY', 'Z_DATA_ERROR'])

module.exports = withTarballStream
function withTarballStream (spec, opts, streamHandler) {
  opts = optCheck(opts)
  spec = npa(spec, opts.where)

  // First, we check for a file: resolved shortcut
  const tryFile = (
    !opts.preferOnline &&
    opts.integrity &&
    opts.resolved &&
    opts.resolved.startsWith('file:')
  )
    ? BB.try(() => {
    // NOTE - this is a special shortcut! Packages installed as files do not
    // have a `resolved` field -- this specific case only occurs when you have,
    // say, a git dependency or a registry dependency that you've packaged into
    // a local file, and put that file: spec in the `resolved` field.
      opts.log.silly('pacote', `trying ${spec} by local file: ${opts.resolved}`)
      const file = path.resolve(opts.where || '.', opts.resolved.substr(5))
      return statAsync(file)
        .then(() => {
          const verifier = ssri.integrityStream({integrity: opts.integrity})
          const stream = fs.createReadStream(file)
            .on('error', err => verifier.emit('error', err))
            .pipe(verifier)
          return streamHandler(stream)
        })
        .catch(err => {
          if (err.code === 'EINTEGRITY') {
            opts.log.warn('pacote', `EINTEGRITY while extracting ${spec} from ${file}.You will have to recreate the file.`)
            opts.log.verbose('pacote', `EINTEGRITY for ${spec}: ${err.message}`)
          }
          throw err
        })
    })
    : BB.reject(Object.assign(new Error('no file!'), {code: 'ENOENT'}))

  const tryDigest = tryFile
    .catch(err => {
      if (
        opts.preferOnline ||
      !opts.cache ||
      !opts.integrity ||
      !RETRIABLE_ERRORS.has(err.code)
      ) {
        throw err
      } else {
        opts.log.silly('tarball', `trying ${spec} by hash: ${opts.integrity}`)
        const stream = cacache.get.stream.byDigest(
          opts.cache, opts.integrity, opts
        )
        stream.once('error', err => stream.on('newListener', (ev, l) => {
          if (ev === 'error') { l(err) }
        }))
        return streamHandler(stream)
          .catch(err => {
            if (err.code === 'EINTEGRITY' || err.code === 'Z_DATA_ERROR') {
              opts.log.warn('tarball', `cached data for ${spec} (${opts.integrity}) seems to be corrupted. Refreshing cache.`)
              return cleanUpCached(opts.cache, opts.integrity, opts)
                .then(() => { throw err })
            } else {
              throw err
            }
          })
      }
    })

  const trySpec = tryDigest
    .catch(err => {
      if (!RETRIABLE_ERRORS.has(err.code)) {
      // If it's not one of our retriable errors, bail out and give up.
        throw err
      } else {
        opts.log.silly(
          'tarball',
          `no local data for ${spec}. Extracting by manifest.`
        )
        return BB.resolve(retry((tryAgain, attemptNum) => {
          const tardata = fetch.tarball(spec, opts)
          if (!opts.resolved) {
            tardata.on('manifest', m => {
              opts.resolved = m._resolved
            })
            tardata.on('integrity', i => {
              opts.integrity = i
            })
          }
          return BB.try(() => streamHandler(tardata))
            .catch(err => {
              // Retry once if we have a cache, to clear up any weird conditions.
              // Don't retry network errors, though -- make-fetch-happen has already
              // taken care of making sure we're all set on that front.
              if (opts.cache && err.code && !err.code.match(/^E\d{3}$/)) {
                if (err.code === 'EINTEGRITY' || err.code === 'Z_DATA_ERROR') {
                  opts.log.warn('tarball', `tarball data for ${spec} (${opts.integrity}) seems to be corrupted. Trying one more time.`)
                }
                return cleanUpCached(opts.cache, err.sri, opts)
                  .then(() => tryAgain(err))
              } else {
                throw err
              }
            })
        }, {retries: 1}))
      }
    })

  return trySpec
    .catch(err => {
      if (err.code === 'EINTEGRITY') {
        err.message = `Verification failed while extracting ${spec}:\n${err.message}`
      }
      throw err
    })
}

function cleanUpCached (cachePath, integrity, opts) {
  return cacache.rm.content(cachePath, integrity, opts)
}
