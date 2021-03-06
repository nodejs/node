'use strict'

const util = require('util')
const fs = require('fs')
const index = require('./lib/entry-index')
const memo = require('./lib/memoization')
const read = require('./lib/content/read')

const Minipass = require('minipass')
const Collect = require('minipass-collect')
const Pipeline = require('minipass-pipeline')

const writeFile = util.promisify(fs.writeFile)

module.exports = function get (cache, key, opts) {
  return getData(false, cache, key, opts)
}
module.exports.byDigest = function getByDigest (cache, digest, opts) {
  return getData(true, cache, digest, opts)
}

function getData (byDigest, cache, key, opts = {}) {
  const { integrity, memoize, size } = opts
  const memoized = byDigest
    ? memo.get.byDigest(cache, key, opts)
    : memo.get(cache, key, opts)
  if (memoized && memoize !== false) {
    return Promise.resolve(
      byDigest
        ? memoized
        : {
          metadata: memoized.entry.metadata,
          data: memoized.data,
          integrity: memoized.entry.integrity,
          size: memoized.entry.size
        }
    )
  }
  return (byDigest ? Promise.resolve(null) : index.find(cache, key, opts)).then(
    (entry) => {
      if (!entry && !byDigest) {
        throw new index.NotFoundError(cache, key)
      }
      return read(cache, byDigest ? key : entry.integrity, {
        integrity,
        size
      })
        .then((data) =>
          byDigest
            ? data
            : {
              data,
              metadata: entry.metadata,
              size: entry.size,
              integrity: entry.integrity
            }
        )
        .then((res) => {
          if (memoize && byDigest) {
            memo.put.byDigest(cache, key, res, opts)
          } else if (memoize) {
            memo.put(cache, entry, res.data, opts)
          }
          return res
        })
    }
  )
}

module.exports.sync = function get (cache, key, opts) {
  return getDataSync(false, cache, key, opts)
}
module.exports.sync.byDigest = function getByDigest (cache, digest, opts) {
  return getDataSync(true, cache, digest, opts)
}

function getDataSync (byDigest, cache, key, opts = {}) {
  const { integrity, memoize, size } = opts
  const memoized = byDigest
    ? memo.get.byDigest(cache, key, opts)
    : memo.get(cache, key, opts)
  if (memoized && memoize !== false) {
    return byDigest
      ? memoized
      : {
        metadata: memoized.entry.metadata,
        data: memoized.data,
        integrity: memoized.entry.integrity,
        size: memoized.entry.size
      }
  }
  const entry = !byDigest && index.find.sync(cache, key, opts)
  if (!entry && !byDigest) {
    throw new index.NotFoundError(cache, key)
  }
  const data = read.sync(cache, byDigest ? key : entry.integrity, {
    integrity: integrity,
    size: size
  })
  const res = byDigest
    ? data
    : {
      metadata: entry.metadata,
      data: data,
      size: entry.size,
      integrity: entry.integrity
    }
  if (memoize && byDigest) {
    memo.put.byDigest(cache, key, res, opts)
  } else if (memoize) {
    memo.put(cache, entry, res.data, opts)
  }
  return res
}

module.exports.stream = getStream

const getMemoizedStream = (memoized) => {
  const stream = new Minipass()
  stream.on('newListener', function (ev, cb) {
    ev === 'metadata' && cb(memoized.entry.metadata)
    ev === 'integrity' && cb(memoized.entry.integrity)
    ev === 'size' && cb(memoized.entry.size)
  })
  stream.end(memoized.data)
  return stream
}

function getStream (cache, key, opts = {}) {
  const { memoize, size } = opts
  const memoized = memo.get(cache, key, opts)
  if (memoized && memoize !== false) {
    return getMemoizedStream(memoized)
  }

  const stream = new Pipeline()
  index
    .find(cache, key)
    .then((entry) => {
      if (!entry) {
        throw new index.NotFoundError(cache, key)
      }
      stream.emit('metadata', entry.metadata)
      stream.emit('integrity', entry.integrity)
      stream.emit('size', entry.size)
      stream.on('newListener', function (ev, cb) {
        ev === 'metadata' && cb(entry.metadata)
        ev === 'integrity' && cb(entry.integrity)
        ev === 'size' && cb(entry.size)
      })

      const src = read.readStream(
        cache,
        entry.integrity,
        { ...opts, size: typeof size !== 'number' ? entry.size : size }
      )

      if (memoize) {
        const memoStream = new Collect.PassThrough()
        memoStream.on('collect', data => memo.put(cache, entry, data, opts))
        stream.unshift(memoStream)
      }
      stream.unshift(src)
    })
    .catch((err) => stream.emit('error', err))

  return stream
}

module.exports.stream.byDigest = getStreamDigest

function getStreamDigest (cache, integrity, opts = {}) {
  const { memoize } = opts
  const memoized = memo.get.byDigest(cache, integrity, opts)
  if (memoized && memoize !== false) {
    const stream = new Minipass()
    stream.end(memoized)
    return stream
  } else {
    const stream = read.readStream(cache, integrity, opts)
    if (!memoize) {
      return stream
    }
    const memoStream = new Collect.PassThrough()
    memoStream.on('collect', data => memo.put.byDigest(
      cache,
      integrity,
      data,
      opts
    ))
    return new Pipeline(stream, memoStream)
  }
}

module.exports.info = info

function info (cache, key, opts = {}) {
  const { memoize } = opts
  const memoized = memo.get(cache, key, opts)
  if (memoized && memoize !== false) {
    return Promise.resolve(memoized.entry)
  } else {
    return index.find(cache, key)
  }
}

module.exports.hasContent = read.hasContent

function cp (cache, key, dest, opts) {
  return copy(false, cache, key, dest, opts)
}

module.exports.copy = cp

function cpDigest (cache, digest, dest, opts) {
  return copy(true, cache, digest, dest, opts)
}

module.exports.copy.byDigest = cpDigest

function copy (byDigest, cache, key, dest, opts = {}) {
  if (read.copy) {
    return (byDigest
      ? Promise.resolve(null)
      : index.find(cache, key, opts)
    ).then((entry) => {
      if (!entry && !byDigest) {
        throw new index.NotFoundError(cache, key)
      }
      return read
        .copy(cache, byDigest ? key : entry.integrity, dest, opts)
        .then(() => {
          return byDigest
            ? key
            : {
              metadata: entry.metadata,
              size: entry.size,
              integrity: entry.integrity
            }
        })
    })
  }

  return getData(byDigest, cache, key, opts).then((res) => {
    return writeFile(dest, byDigest ? res : res.data).then(() => {
      return byDigest
        ? key
        : {
          metadata: res.metadata,
          size: res.size,
          integrity: res.integrity
        }
    })
  })
}
