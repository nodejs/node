'use strict'

var fs = require('graceful-fs')
var path = require('path')
var mkdir = require('mkdirp')
var chownr = require('chownr')
var npm = require('../npm.js')
var log = require('npmlog')
var cacheFile = require('npm-cache-filename')
var getCacheStat = require('../cache/get-stat.js')
var mapToRegistry = require('../utils/map-to-registry.js')
var jsonstream = require('JSONStream')
var writeStreamAtomic = require('fs-write-stream-atomic')
var ms = require('mississippi')
var sortedUnionStream = require('sorted-union-stream')
var once = require('once')

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
function allPackageMetadata (staleness) {
  var stream = ms.through.obj()

  mapToRegistry('-/all', npm.config, function (er, uri, auth) {
    if (er) return stream.emit('error', er)

    var cacheBase = cacheFile(npm.config.get('cache'))(uri)
    var cachePath = path.join(cacheBase, '.cache.json')

    createEntryStream(cachePath, uri, auth, staleness, function (err, entryStream, latest, newEntries) {
      if (err) return stream.emit('error', err)
      log.silly('all-package-metadata', 'entry stream created')
      if (entryStream && newEntries) {
        createCacheWriteStream(cachePath, latest, function (err, writeStream) {
          if (err) return stream.emit('error', err)
          log.silly('all-package-metadata', 'output stream created')
          ms.pipeline.obj(entryStream, writeStream, stream)
        })
      } else if (entryStream) {
        ms.pipeline.obj(entryStream, stream)
      } else {
        stream.emit('error', new Error('No search sources available'))
      }
    })
  })
  return stream
}

// Creates a stream of the latest available package metadata.
// Metadata will come from a combination of the local cache and remote data.
module.exports._createEntryStream = createEntryStream
function createEntryStream (cachePath, uri, auth, staleness, cb) {
  createCacheEntryStream(cachePath, function (err, cacheStream, cacheLatest) {
    cacheLatest = cacheLatest || 0
    if (err) {
      log.warn('', 'Failed to read search cache. Rebuilding')
      log.silly('all-package-metadata', 'cache read error: ', err)
    }
    createEntryUpdateStream(uri, auth, staleness, cacheLatest, function (err, updateStream, updatedLatest) {
      updatedLatest = updatedLatest || 0
      var latest = updatedLatest || cacheLatest
      if (!cacheStream && !updateStream) {
        return cb(new Error('No search sources available'))
      }
      if (err) {
        log.warn('', 'Search data request failed, search might be stale')
        log.silly('all-package-metadata', 'update request error: ', err)
      }
      if (cacheStream && updateStream) {
        // Deduped, unioned, sorted stream from the combination of both.
        cb(null,
          createMergedStream(cacheStream, updateStream),
          latest,
          !!updatedLatest)
      } else {
        // Either one works if one or the other failed
        cb(null, cacheStream || updateStream, latest, !!updatedLatest)
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
  return sortedUnionStream(b, a, function (pkg) { return pkg.name })
}

// Reads the local index and returns a stream that spits out package data.
module.exports._createCacheEntryStream = createCacheEntryStream
function createCacheEntryStream (cacheFile, cb) {
  log.verbose('all-package-metadata', 'creating entry stream from local cache')
  log.verbose('all-package-metadata', cacheFile)
  fs.stat(cacheFile, function (err, stat) {
    if (err) return cb(err)
    // TODO - This isn't very helpful if `cacheFile` is empty or just `{}`
    var entryStream = ms.pipeline.obj(
      fs.createReadStream(cacheFile),
      jsonstream.parse('*'),
      // I believe this passthrough is necessary cause `jsonstream` returns
      // weird custom streams that behave funny sometimes.
      ms.through.obj()
    )
    extractUpdated(entryStream, 'cached-entry-stream', cb)
  })
}

// Stream of entry updates from the server. If `latest` is `0`, streams the
// entire metadata object from the registry.
module.exports._createEntryUpdateStream = createEntryUpdateStream
function createEntryUpdateStream (all, auth, staleness, latest, cb) {
  log.verbose('all-package-metadata', 'creating remote entry stream')
  var params = {
    timeout: 600,
    follow: true,
    staleOk: true,
    auth: auth,
    streaming: true
  }
  var partialUpdate = false
  if (latest && (Date.now() - latest < (staleness * 1000))) {
    // Skip the request altogether if our `latest` isn't stale.
    log.verbose('all-package-metadata', 'Local data up to date, skipping update')
    return cb(null)
  } else if (latest === 0) {
    log.warn('', 'Building the local index for the first time, please be patient')
    log.verbose('all-package-metadata', 'No cached data: requesting full metadata db')
  } else {
    log.verbose('all-package-metadata', 'Cached data present with timestamp:', latest, 'requesting partial index update')
    all += '/since?stale=update_after&startkey=' + latest
    partialUpdate = true
  }
  npm.registry.request(all, params, function (er, res) {
    if (er) return cb(er)
    log.silly('all-package-metadata', 'request stream opened, code:', res.statusCode)
    // NOTE - The stream returned by `request` seems to be very persnickety
    //        and this is almost a magic incantation to get it to work.
    //        Modify how `res` is used here at your own risk.
    var entryStream = ms.pipeline.obj(
      res,
      ms.through(function (chunk, enc, cb) {
        cb(null, chunk)
      }),
      jsonstream.parse('*', function (pkg, key) {
        if (key[0] === '_updated' || key[0][0] !== '_') {
          return pkg
        }
      })
    )
    if (partialUpdate) {
      // The `/all/since` endpoint doesn't return `_updated`, so we
      // just use the request's own timestamp.
      cb(null, entryStream, Date.parse(res.headers.date))
    } else {
      extractUpdated(entryStream, 'entry-update-stream', cb)
    }
  })
}

// Both the (full) remote requests and the local index have `_updated` as their
// first returned entries. This is the "latest" unix timestamp for the metadata
// in question. This code does a bit of juggling with the data streams
// so that we can pretend that field doesn't exist, but still extract `latest`
function extractUpdated (entryStream, label, cb) {
  cb = once(cb)
  log.silly('all-package-metadata', 'extracting latest')
  function nope (msg) {
    return function () {
      log.warn('all-package-metadata', label, msg)
      entryStream.removeAllListeners()
      entryStream.destroy()
      cb(new Error(msg))
    }
  }
  var onErr = nope('Failed to read stream')
  var onEnd = nope('Empty or invalid stream')
  entryStream.on('error', onErr)
  entryStream.on('end', onEnd)
  entryStream.once('data', function (latest) {
    log.silly('all-package-metadata', 'got first stream entry for', label, latest)
    entryStream.removeListener('error', onErr)
    entryStream.removeListener('end', onEnd)
    // Because `.once()` unpauses the stream, we re-pause it after the first
    // entry so we don't vomit entries into the void.
    entryStream.pause()
    if (typeof latest === 'number') {
      // The extra pipeline is to return a stream that will implicitly unpause
      // after having an `.on('data')` listener attached, since using this
      // `data` event broke its initial state.
      cb(null, ms.pipeline.obj(entryStream, ms.through.obj()), latest)
    } else {
      cb(new Error('expected first entry to be _updated'))
    }
  })
}

// Creates a stream that writes input metadata to the current cache.
// Cache updates are atomic, and the stream closes when *everything* is done.
// The stream is also passthrough, so entries going through it will also
// be output from it.
module.exports._createCacheWriteStream = createCacheWriteStream
function createCacheWriteStream (cacheFile, latest, cb) {
  _ensureCacheDirExists(cacheFile, function (err) {
    if (err) return cb(err)
    log.silly('all-package-metadata', 'creating output stream')
    var outStream = _createCacheOutStream()
    var cacheFileStream = writeStreamAtomic(cacheFile)
    var inputStream = _createCacheInStream(cacheFileStream, outStream, latest)

    // Glue together the various streams so they fail together.
    // `cacheFileStream` errors are already handled by the `inputStream`
    // pipeline
    var errEmitted = false
    linkStreams(inputStream, outStream, function () { errEmitted = true })

    cacheFileStream.on('close', function () { !errEmitted && outStream.end() })

    cb(null, ms.duplex.obj(inputStream, outStream))
  })
}

function _ensureCacheDirExists (cacheFile, cb) {
  var cacheBase = path.dirname(cacheFile)
  log.silly('all-package-metadata', 'making sure cache dir exists at', cacheBase)
  getCacheStat(function (er, st) {
    if (er) return cb(er)
    mkdir(cacheBase, function (er, made) {
      if (er) return cb(er)
      chownr(made || cacheBase, st.uid, st.gid, cb)
    })
  })
}

function _createCacheOutStream () {
  return ms.pipeline.obj(
    // These two passthrough `through` streams compensate for some
    // odd behavior with `jsonstream`.
    ms.through(),
    jsonstream.parse('*', function (obj, key) {
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
  var updatedWritten = false
  var inStream = ms.pipeline.obj(
    ms.through.obj(function (pkg, enc, cb) {
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
    jsonstream.stringifyObject('{', ',', '}'),
    ms.through(function (chunk, enc, cb) {
      // This tees off the buffer data to `outStream`, and then continues
      // the pipeline as usual
      outStream.write(chunk, enc, function () {
        cb(null, chunk)
      })
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
      cb(err)
    }
  })
  b.on('error', function (err) {
    if (err !== lastError) {
      lastError = err
      a.emit('error', err)
      cb(err)
    }
  })
}
