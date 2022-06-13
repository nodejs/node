'use strict'

const fs = require('@npmcli/fs')
const fsm = require('fs-minipass')
const ssri = require('ssri')
const contentPath = require('./path')
const Pipeline = require('minipass-pipeline')

module.exports = read

const MAX_SINGLE_READ_SIZE = 64 * 1024 * 1024
async function read (cache, integrity, opts = {}) {
  const { size } = opts
  const { stat, cpath, sri } = await withContentSri(cache, integrity, async (cpath, sri) => {
    // get size
    const stat = await fs.lstat(cpath)
    return { stat, cpath, sri }
  })
  if (typeof size === 'number' && stat.size !== size) {
    throw sizeError(size, stat.size)
  }

  if (stat.size > MAX_SINGLE_READ_SIZE) {
    return readPipeline(cpath, stat.size, sri, new Pipeline()).concat()
  }

  const data = await fs.readFile(cpath, { encoding: null })
  if (!ssri.checkData(data, sri)) {
    throw integrityError(sri, cpath)
  }

  return data
}

const readPipeline = (cpath, size, sri, stream) => {
  stream.push(
    new fsm.ReadStream(cpath, {
      size,
      readSize: MAX_SINGLE_READ_SIZE,
    }),
    ssri.integrityStream({
      integrity: sri,
      size,
    })
  )
  return stream
}

module.exports.sync = readSync

function readSync (cache, integrity, opts = {}) {
  const { size } = opts
  return withContentSriSync(cache, integrity, (cpath, sri) => {
    const data = fs.readFileSync(cpath, { encoding: null })
    if (typeof size === 'number' && size !== data.length) {
      throw sizeError(size, data.length)
    }

    if (ssri.checkData(data, sri)) {
      return data
    }

    throw integrityError(sri, cpath)
  })
}

module.exports.stream = readStream
module.exports.readStream = readStream

function readStream (cache, integrity, opts = {}) {
  const { size } = opts
  const stream = new Pipeline()
  // Set all this up to run on the stream and then just return the stream
  Promise.resolve().then(async () => {
    const { stat, cpath, sri } = await withContentSri(cache, integrity, async (cpath, sri) => {
      // just lstat to ensure it exists
      const stat = await fs.lstat(cpath)
      return { stat, cpath, sri }
    })
    if (typeof size === 'number' && size !== stat.size) {
      return stream.emit('error', sizeError(size, stat.size))
    }

    readPipeline(cpath, stat.size, sri, stream)
  }).catch(err => stream.emit('error', err))

  return stream
}

module.exports.copy = copy
module.exports.copy.sync = copySync

function copy (cache, integrity, dest) {
  return withContentSri(cache, integrity, (cpath, sri) => {
    return fs.copyFile(cpath, dest)
  })
}

function copySync (cache, integrity, dest) {
  return withContentSriSync(cache, integrity, (cpath, sri) => {
    return fs.copyFileSync(cpath, dest)
  })
}

module.exports.hasContent = hasContent

async function hasContent (cache, integrity) {
  if (!integrity) {
    return false
  }

  try {
    return await withContentSri(cache, integrity, async (cpath, sri) => {
      const stat = await fs.lstat(cpath)
      return { size: stat.size, sri, stat }
    })
  } catch (err) {
    if (err.code === 'ENOENT') {
      return false
    }

    if (err.code === 'EPERM') {
      /* istanbul ignore else */
      if (process.platform !== 'win32') {
        throw err
      } else {
        return false
      }
    }
  }
}

module.exports.hasContent.sync = hasContentSync

function hasContentSync (cache, integrity) {
  if (!integrity) {
    return false
  }

  return withContentSriSync(cache, integrity, (cpath, sri) => {
    try {
      const stat = fs.lstatSync(cpath)
      return { size: stat.size, sri, stat }
    } catch (err) {
      if (err.code === 'ENOENT') {
        return false
      }

      if (err.code === 'EPERM') {
        /* istanbul ignore else */
        if (process.platform !== 'win32') {
          throw err
        } else {
          return false
        }
      }
    }
  })
}

async function withContentSri (cache, integrity, fn) {
  const sri = ssri.parse(integrity)
  // If `integrity` has multiple entries, pick the first digest
  // with available local data.
  const algo = sri.pickAlgorithm()
  const digests = sri[algo]

  if (digests.length <= 1) {
    const cpath = contentPath(cache, digests[0])
    return fn(cpath, digests[0])
  } else {
    // Can't use race here because a generic error can happen before
    // a ENOENT error, and can happen before a valid result
    const results = await Promise.all(digests.map(async (meta) => {
      try {
        return await withContentSri(cache, meta, fn)
      } catch (err) {
        if (err.code === 'ENOENT') {
          return Object.assign(
            new Error('No matching content found for ' + sri.toString()),
            { code: 'ENOENT' }
          )
        }
        return err
      }
    }))
    // Return the first non error if it is found
    const result = results.find((r) => !(r instanceof Error))
    if (result) {
      return result
    }

    // Throw the No matching content found error
    const enoentError = results.find((r) => r.code === 'ENOENT')
    if (enoentError) {
      throw enoentError
    }

    // Throw generic error
    throw results.find((r) => r instanceof Error)
  }
}

function withContentSriSync (cache, integrity, fn) {
  const sri = ssri.parse(integrity)
  // If `integrity` has multiple entries, pick the first digest
  // with available local data.
  const algo = sri.pickAlgorithm()
  const digests = sri[algo]
  if (digests.length <= 1) {
    const cpath = contentPath(cache, digests[0])
    return fn(cpath, digests[0])
  } else {
    let lastErr = null
    for (const meta of digests) {
      try {
        return withContentSriSync(cache, meta, fn)
      } catch (err) {
        lastErr = err
      }
    }
    throw lastErr
  }
}

function sizeError (expected, found) {
  /* eslint-disable-next-line max-len */
  const err = new Error(`Bad data size: expected inserted data to be ${expected} bytes, but got ${found} instead`)
  err.expected = expected
  err.found = found
  err.code = 'EBADSIZE'
  return err
}

function integrityError (sri, path) {
  const err = new Error(`Integrity verification failed for ${sri} (${path})`)
  err.code = 'EINTEGRITY'
  err.sri = sri
  err.path = path
  return err
}
