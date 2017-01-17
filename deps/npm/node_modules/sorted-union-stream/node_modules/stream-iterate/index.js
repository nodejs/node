var Readable = require('stream').Readable

var stream2 = function (stream) {
  if (stream._readableState) return stream
  return new Readable({objectMode: true, highWaterMark: 16}).wrap(stream)
}

module.exports = function (stream) {
  stream = stream2(stream)

  var ended = false
  var data = null
  var err = null
  var destroyed = false
  var fn = null

  var consume = function (e) {
    if (e) {
      destroyed = true
      if (stream.destroy) stream.destroy(e)
      return
    }

    data = null
    err = null
  }

  var onresult = function () {
    if (!fn) return
    var tmp = fn
    fn = undefined
    tmp(err, data, consume)
  }

  var update = function () {
    if (!fn) return
    data = stream.read()
    if (data === null && !ended) return
    onresult()
  }

  var onend = function () {
    ended = true
    onresult()
  }

  stream.on('readable', update)

  stream.on('error', function (e) {
    err = e
    onresult()
  })

  stream.on('close', function () {
    if (stream._readableState.ended) return
    onend()
  })

  stream.on('end', onend)

  return function (callback) {
    if (destroyed) return
    if (err) return callback(err, null, consume)
    if (data) return callback(null, data, consume)
    if (ended) return callback(null, null, consume)
    fn = callback
    update()
  }
}
