'use strict'

const { pipeline } = require('./pipeline')

const Duplex = require('./duplex')

const { destroyer } = require('./destroy')

const { isNodeStream, isReadable, isWritable } = require('./utils')

const {
  AbortError,
  codes: { ERR_INVALID_ARG_VALUE, ERR_MISSING_ARGS }
} = require('../../ours/errors')

module.exports = function compose(...streams) {
  if (streams.length === 0) {
    throw new ERR_MISSING_ARGS('streams')
  }

  if (streams.length === 1) {
    return Duplex.from(streams[0])
  }

  const orgStreams = [...streams]

  if (typeof streams[0] === 'function') {
    streams[0] = Duplex.from(streams[0])
  }

  if (typeof streams[streams.length - 1] === 'function') {
    const idx = streams.length - 1
    streams[idx] = Duplex.from(streams[idx])
  }

  for (let n = 0; n < streams.length; ++n) {
    if (!isNodeStream(streams[n])) {
      // TODO(ronag): Add checks for non streams.
      continue
    }

    if (n < streams.length - 1 && !isReadable(streams[n])) {
      throw new ERR_INVALID_ARG_VALUE(`streams[${n}]`, orgStreams[n], 'must be readable')
    }

    if (n > 0 && !isWritable(streams[n])) {
      throw new ERR_INVALID_ARG_VALUE(`streams[${n}]`, orgStreams[n], 'must be writable')
    }
  }

  let ondrain
  let onfinish
  let onreadable
  let onclose
  let d

  function onfinished(err) {
    const cb = onclose
    onclose = null

    if (cb) {
      cb(err)
    } else if (err) {
      d.destroy(err)
    } else if (!readable && !writable) {
      d.destroy()
    }
  }

  const head = streams[0]
  const tail = pipeline(streams, onfinished)
  const writable = !!isWritable(head)
  const readable = !!isReadable(tail) // TODO(ronag): Avoid double buffering.
  // Implement Writable/Readable/Duplex traits.
  // See, https://github.com/nodejs/node/pull/33515.

  d = new Duplex({
    // TODO (ronag): highWaterMark?
    writableObjectMode: !!(head !== null && head !== undefined && head.writableObjectMode),
    readableObjectMode: !!(tail !== null && tail !== undefined && tail.writableObjectMode),
    writable,
    readable
  })

  if (writable) {
    d._write = function (chunk, encoding, callback) {
      if (head.write(chunk, encoding)) {
        callback()
      } else {
        ondrain = callback
      }
    }

    d._final = function (callback) {
      head.end()
      onfinish = callback
    }

    head.on('drain', function () {
      if (ondrain) {
        const cb = ondrain
        ondrain = null
        cb()
      }
    })
    tail.on('finish', function () {
      if (onfinish) {
        const cb = onfinish
        onfinish = null
        cb()
      }
    })
  }

  if (readable) {
    tail.on('readable', function () {
      if (onreadable) {
        const cb = onreadable
        onreadable = null
        cb()
      }
    })
    tail.on('end', function () {
      d.push(null)
    })

    d._read = function () {
      while (true) {
        const buf = tail.read()

        if (buf === null) {
          onreadable = d._read
          return
        }

        if (!d.push(buf)) {
          return
        }
      }
    }
  }

  d._destroy = function (err, callback) {
    if (!err && onclose !== null) {
      err = new AbortError()
    }

    onreadable = null
    ondrain = null
    onfinish = null

    if (onclose === null) {
      callback(err)
    } else {
      onclose = callback
      destroyer(tail, err)
    }
  }

  return d
}
