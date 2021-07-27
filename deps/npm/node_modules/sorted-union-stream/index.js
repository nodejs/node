var iterate = require('stream-iterate')
var from = require('from2')

var defaultKey = function (val) {
  return val.key || val
}

var union = function (streamA, streamB, toKey) {
  var readA = iterate(streamA)
  var readB = iterate(streamB)

  if (!toKey) toKey = defaultKey

  var stream = from.obj(function loop (size, cb) {
    readA(function (err, dataA, nextA) {
      if (err) return cb(err)
      readB(function (err, dataB, nextB) {
        if (err) return cb(err)

        if (!dataA && !dataB) return cb(null, null)

        if (!dataA) {
          nextB()
          return cb(null, dataB)
        }

        if (!dataB) {
          nextA()
          return cb(null, dataA)
        }

        var keyA = toKey(dataA)
        var keyB = toKey(dataB)

        if (keyA === keyB) {
          nextB()
          return loop(size, cb)
        }

        if (keyA < keyB) {
          nextA()
          return cb(null, dataA)
        }

        nextB()
        cb(null, dataB)
      })
    })
  })

  stream.on('close', function () {
    if (streamA.destroy) streamA.destroy()
    if (streamB.destroy) streamB.destroy()
  })

  return stream
}

module.exports = union
