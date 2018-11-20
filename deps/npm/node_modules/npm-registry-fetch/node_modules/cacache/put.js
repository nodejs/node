'use strict'

const index = require('./lib/entry-index')
const memo = require('./lib/memoization')
const write = require('./lib/content/write')
const to = require('mississippi').to

module.exports = putData
function putData (cache, key, data, opts) {
  opts = opts || {}
  return write(cache, data, opts).then(res => {
    // TODO - stop modifying opts
    opts.size = res.size
    return index.insert(cache, key, res.integrity, opts).then(entry => {
      if (opts.memoize) {
        memo.put(cache, entry, data, opts)
      }
      return res.integrity
    })
  })
}

module.exports.stream = putStream
function putStream (cache, key, opts) {
  opts = opts || {}
  let integrity
  let size
  const contentStream = write.stream(
    cache, opts
  ).on('integrity', int => {
    integrity = int
  }).on('size', s => {
    size = s
  })
  let memoData
  let memoTotal = 0
  const stream = to((chunk, enc, cb) => {
    contentStream.write(chunk, enc, () => {
      if (opts.memoize) {
        if (!memoData) { memoData = [] }
        memoData.push(chunk)
        memoTotal += chunk.length
      }
      cb()
    })
  }, cb => {
    contentStream.end(() => {
      // TODO - stop modifying `opts`
      opts.size = size
      index.insert(cache, key, integrity, opts).then(entry => {
        if (opts.memoize) {
          memo.put(cache, entry, Buffer.concat(memoData, memoTotal), opts)
        }
        stream.emit('integrity', integrity)
        cb()
      })
    })
  })
  let erred = false
  stream.once('error', err => {
    if (erred) { return }
    erred = true
    contentStream.emit('error', err)
  })
  contentStream.once('error', err => {
    if (erred) { return }
    erred = true
    stream.emit('error', err)
  })
  return stream
}
