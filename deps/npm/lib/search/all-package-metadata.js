'use strict'

const BB = require('bluebird')

const cacheFile = require('npm-cache-filename')
const chownr = BB.promisify(require('chownr'))
const correctMkdir = BB.promisify(require('../utils/correct-mkdir.js'))
const figgyPudding = require('figgy-pudding')
const fs = require('graceful-fs')
const JSONStream = require('JSONStream')
const log = require('npmlog')
const mkdir = BB.promisify(require('gentle-fs').mkdir)
const ms = require('mississippi')
const npmFetch = require('libnpm/fetch')
const path = require('path')
const sortedUnionStream = require('sorted-union-stream')
const url = require('url')
const writeStreamAtomic = require('fs-write-stream-atomic')

const statAsync = BB.promisify(fs.stat)

const APMOpts = figgyPudding({
  cache: {},
  registry: {}
})
// Returns a sorted stream of all package metadata. Internally, takes care of
// maintaining its metadata cache and making partial or full remote requests,
// according to staleness, validity, etc.
//
// The local cache must hold certain invariants:
// 1. It must be a proper JSON object
// 2. It must have its keys lexically sorted
// 3. The first entry must be `_updated` with a millisecond timestamp as a val.
// 4. It must include all entries that exist in the metadata endpoint as of
//    the value in `_updated`
module.exports = allPackageMetadata
function allPackageMetadata (opts) {
  const staleness = opts.staleness
  const stream = ms.through.obj()

  opts = APMOpts(opts)
  const cacheBase = cacheFile(path.resolve(path.dirname(opts.cache)))(url.resolve(opts.registry, '/-/all'))
  const cachePath = path.join(cacheBase, '.cache.json')
  createEntryStream(
    cachePath, staleness, opts
  ).then(({entryStream, latest, newEntries}) => {
    log.silly('all-package-metadata', 'entry stream created')
    if (entryStream && newEntries) {
      return createCacheWriteStream(cachePath, latest, opts).then(writer => {
        log.silly('all-package-metadata', 'output stream created')
        ms.pipeline.obj(entryStream, writer, stream)
      })
    } else if (entryStream) {
      ms.pipeline.obj(entryStream, stream)
    } else {
      stream.emit('error', new Error('No search sources available'))
    }
  }).catch(err => stream.emit('error', err))
  return stream
}

// Creates a stream of the latest available package metadata.
// Metadata will come from a combination of the local cache and remote data.
module.exports._createEntryStream = createEntryStream
function createEntryStream (cachePath, staleness, opts) {
  return createCacheEntryStream(
    cachePath, opts
  ).catch(err => {
    log.warn('', 'Failed to read search cache. Rebuilding')
    log.silly('all-package-metadata', 'cache read error: ', err)
    return {}
  }).then(({
    updateStream: cacheStream,
    updatedLatest: cacheLatest
  }) => {
    cacheLatest = cacheLatest || 0
    return createEntryUpdateStream(staleness, cacheLatest, opts).catch(err => {
      log.warn('', 'Search data request failed, search might be stale')
      log.silly('all-package-metadata', 'update request error: ', err)
      return {}
    }).then(({updateStream, updatedLatest}) => {
      updatedLatest = updatedLatest || 0
      const latest = updatedLatest || cacheLatest
      if (!cacheStream && !updateStream) {
        throw new Error('No search sources available')
      }
      if (cacheStream && updateStream) {
        // Deduped, unioned, sorted stream from the combination of both.
        return {
          entryStream: createMergedStream(cacheStream, updateStream),
          latest,
          newEntries: !!updatedLatest
        }
      } else {
        // Either one works if one or the other failed
        return {
          entryStream: cacheStream || updateStream,
          latest,
          newEntries: !!updatedLatest
        }
      }
    })
  })
}

// Merges `a` and `b` into one stream, dropping duplicates in favor of entries
// in `b`. Both input streams should already be individually sorted, and the
// returned output stream will have semantics resembling the merge step of a
// plain old merge sort.
module.exports._createMergedStream = createMergedStream
function createMergedStream (a, b) {
  linkStreams(a, b)
  return sortedUnionStream(b, a, ({name}) => name)
}

// Reads the local index and returns a stream that spits out package data.
module.exports._createCacheEntryStream = createCacheEntryStream
function createCacheEntryStream (cacheFile, opts) {
  log.verbose('all-package-metadata', 'creating entry stream from local cache')
  log.verbose('all-package-metadata', cacheFile)
  return statAsync(cacheFile).then(stat => {
    // TODO - This isn't very helpful if `cacheFile` is empty or just `{}`
    const entryStream = ms.pipeline.obj(
      fs.createReadStream(cacheFile),
      JSONStream.parse('*'),
      // I believe this passthrough is necessary cause `jsonstream` returns
      // weird custom streams that behave funny sometimes.
      ms.through.obj()
    )
    return extractUpdated(entryStream, 'cached-entry-stream', opts)
  })
}

// Stream of entry updates from the server. If `latest` is `0`, streams the
// entire metadata object from the registry.
module.exports._createEntryUpdateStream = createEntryUpdateStream
function createEntryUpdateStream (staleness, latest, opts) {
  log.verbose('all-package-metadata', 'creating remote entry stream')
  let partialUpdate = false
  let uri = '/-/all'
  if (latest && (Date.now() - latest < (staleness * 1000))) {
    // Skip the request altogether if our `latest` isn't stale.
    log.verbose('all-package-metadata', 'Local data up to date, skipping update')
    return BB.resolve({})
  } else if (latest === 0) {
    log.warn('', 'Building the local index for the first time, please be patient')
    log.verbose('all-package-metadata', 'No cached data: requesting full metadata db')
  } else {
    log.verbose('all-package-metadata', 'Cached data present with timestamp:', latest, 'requesting partial index update')
    uri += '/since?stale=update_after&startkey=' + latest
    partialUpdate = true
  }
  return npmFetch(uri, opts).then(res => {
    log.silly('all-package-metadata', 'request stream opened, code:', res.statusCode)
    let entryStream = ms.pipeline.obj(
      res.body,
      JSONStream.parse('*', (pkg, key) => {
        if (key[0] === '_updated' || key[0][0] !== '_') {
          return pkg
        }
      })
    )
    if (partialUpdate) {
      // The `/all/since` endpoint doesn't return `_updated`, so we
      // just use the request's own timestamp.
      return {
        updateStream: entryStream,
        updatedLatest: Date.parse(res.headers.get('date'))
      }
    } else {
      return extractUpdated(entryStream, 'entry-update-stream', opts)
    }
  })
}

// Both the (full) remote requests and the local index have `_updated` as their
// first returned entries. This is the "latest" unix timestamp for the metadata
// in question. This code does a bit of juggling with the data streams
// so that we can pretend that field doesn't exist, but still extract `latest`
function extractUpdated (entryStream, label, opts) {
  log.silly('all-package-metadata', 'extracting latest')
  return new BB((resolve, reject) => {
    function nope (msg) {
      return function () {
        log.warn('all-package-metadata', label, msg)
        entryStream.removeAllListeners()
        entryStream.destroy()
        reject(new Error(msg))
      }
    }
    const onErr = nope('Failed to read stream')
    const onEnd = nope('Empty or invalid stream')
    entryStream.on('error', onErr)
    entryStream.on('end', onEnd)
    entryStream.once('data', latest => {
      log.silly('all-package-metadata', 'got first stream entry for', label, latest)
      entryStream.removeListener('error', onErr)
      entryStream.removeListener('end', onEnd)
      if (typeof latest === 'number') {
        // The extra pipeline is to return a stream that will implicitly unpause
        // after having an `.on('data')` listener attached, since using this
        // `data` event broke its initial state.
        resolve({
          updateStream: entryStream.pipe(ms.through.obj()),
          updatedLatest: latest
        })
      } else {
        reject(new Error('expected first entry to be _updated'))
      }
    })
  })
}

// Creates a stream that writes input metadata to the current cache.
// Cache updates are atomic, and the stream closes when *everything* is done.
// The stream is also passthrough, so entries going through it will also
// be output from it.
module.exports._createCacheWriteStream = createCacheWriteStream
function createCacheWriteStream (cacheFile, latest, opts) {
  return _ensureCacheDirExists(cacheFile, opts).then(({uid, gid}) => {
    log.silly('all-package-metadata', 'creating output stream')
    const outStream = _createCacheOutStream()
    const cacheFileStream = writeStreamAtomic(cacheFile)
    const inputStream = _createCacheInStream(
      cacheFileStream, outStream, latest
    )

    // Glue together the various streams so they fail together.
    // `cacheFileStream` errors are already handled by the `inputStream`
    // pipeline
    let errEmitted = false
    linkStreams(inputStream, outStream, () => { errEmitted = true })

    cacheFileStream.on('close', () => {
      if (!errEmitted) {
        if (typeof uid === 'number' &&
            typeof gid === 'number' &&
            process.getuid &&
            process.getgid &&
            (process.getuid() !== uid || process.getgid() !== gid)) {
          chownr.sync(cacheFile, uid, gid)
        }
        outStream.end()
      }
    })

    return ms.duplex.obj(inputStream, outStream)
  })
}

// return the {uid,gid} that the cache should have
function _ensureCacheDirExists (cacheFile, opts) {
  var cacheBase = path.dirname(cacheFile)
  log.silly('all-package-metadata', 'making sure cache dir exists at', cacheBase)
  return correctMkdir(opts.cache).then(st => {
    return mkdir(cacheBase).then(made => {
      return chownr(made || cacheBase, st.uid, st.gid)
    }).then(() => ({ uid: st.uid, gid: st.gid }))
  })
}

function _createCacheOutStream () {
  // NOTE: this looks goofy, but it's necessary in order to get
  //       JSONStream to play nice with the rest of everything.
  return ms.pipeline.obj(
    ms.through(),
    JSONStream.parse('*', (obj, key) => {
      // This stream happens to get _updated passed through it, for
      // implementation reasons. We make sure to filter it out cause
      // the fact that it comes t
      if (typeof obj === 'object') {
        return obj
      }
    }),
    ms.through.obj()
  )
}

function _createCacheInStream (writer, outStream, latest) {
  let updatedWritten = false
  const inStream = ms.pipeline.obj(
    ms.through.obj((pkg, enc, cb) => {
      if (!updatedWritten && typeof pkg === 'number') {
        // This is the `_updated` value getting sent through.
        updatedWritten = true
        return cb(null, ['_updated', pkg])
      } else if (typeof pkg !== 'object') {
        this.emit('error', new Error('invalid value written to input stream'))
      } else {
        // The [key, val] format is expected by `jsonstream` for object writing
        cb(null, [pkg.name, pkg])
      }
    }),
    JSONStream.stringifyObject('{', ',', '}'),
    ms.through((chunk, enc, cb) => {
      // This tees off the buffer data to `outStream`, and then continues
      // the pipeline as usual
      outStream.write(chunk, enc, () => cb(null, chunk))
    }),
    // And finally, we write to the cache file.
    writer
  )
  inStream.write(latest)
  return inStream
}

// Links errors between `a` and `b`, preventing cycles, and calls `cb` if
// an error happens, once per error.
function linkStreams (a, b, cb) {
  var lastError = null
  a.on('error', function (err) {
    if (err !== lastError) {
      lastError = err
      b.emit('error', err)
      cb && cb(err)
    }
  })
  b.on('error', function (err) {
    if (err !== lastError) {
      lastError = err
      a.emit('error', err)
      cb && cb(err)
    }
  })
}
