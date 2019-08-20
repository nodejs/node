'use strict'

const BB = require('bluebird')

const extractStream = require('./lib/extract-stream.js')
const fs = require('fs')
const mkdirp = BB.promisify(require('mkdirp'))
const npa = require('npm-package-arg')
const optCheck = require('./lib/util/opt-check.js')
const path = require('path')
const rimraf = BB.promisify(require('rimraf'))
const withTarballStream = require('./lib/with-tarball-stream.js')
const inferOwner = require('infer-owner')
const chown = BB.promisify(require('chownr'))

const truncateAsync = BB.promisify(fs.truncate)
const readFileAsync = BB.promisify(fs.readFile)
const appendFileAsync = BB.promisify(fs.appendFile)

// you used to call me on my...
const selfOwner = process.getuid ? {
  uid: process.getuid(),
  gid: process.getgid()
} : {
  uid: undefined,
  gid: undefined
}

module.exports = extract
function extract (spec, dest, opts) {
  opts = optCheck(opts)
  spec = npa(spec, opts.where)
  if (spec.type === 'git' && !opts.cache) {
    throw new TypeError('Extracting git packages requires a cache folder')
  }
  if (typeof dest !== 'string') {
    throw new TypeError('Extract requires a destination')
  }
  const startTime = Date.now()
  return inferOwner(dest).then(({ uid, gid }) => {
    opts = opts.concat({ uid, gid })
    return withTarballStream(spec, opts, stream => {
      return tryExtract(spec, stream, dest, opts)
    })
      .then(() => {
        if (!opts.resolved) {
          const pjson = path.join(dest, 'package.json')
          return readFileAsync(pjson, 'utf8')
            .then(str => truncateAsync(pjson)
              .then(() => appendFileAsync(pjson, str.replace(
                /}\s*$/,
                `\n,"_resolved": ${
                  JSON.stringify(opts.resolved || '')
                }\n,"_integrity": ${
                  JSON.stringify(opts.integrity || '')
                }\n,"_from": ${
                  JSON.stringify(spec.toString())
                }\n}`
              ))))
        }
      })
      .then(() => opts.log.silly(
        'extract',
        `${spec} extracted to ${dest} (${Date.now() - startTime}ms)`
      ))
  })
}

function tryExtract (spec, tarStream, dest, opts) {
  return new BB((resolve, reject) => {
    tarStream.on('error', reject)

    rimraf(dest)
      .then(() => mkdirp(dest))
      .then((made) => {
        // respect the current ownership of unpack targets
        // but don't try to chown if we're not root.
        if (selfOwner.uid === 0 &&
            typeof selfOwner.gid === 'number' &&
            selfOwner.uid !== opts.uid && selfOwner.gid !== opts.gid) {
          return chown(made || dest, opts.uid, opts.gid)
        }
      })
      .then(() => {
        const xtractor = extractStream(spec, dest, opts)
        xtractor.on('error', reject)
        xtractor.on('close', resolve)
        tarStream.pipe(xtractor)
      })
      .catch(reject)
  })
    .catch(err => {
      if (err.code === 'EINTEGRITY') {
        err.message = `Verification failed while extracting ${spec}:\n${err.message}`
      }

      throw err
    })
}
