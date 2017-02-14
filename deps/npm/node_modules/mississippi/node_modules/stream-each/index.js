var eos = require('end-of-stream')

module.exports = each

function each (stream, fn, cb) {
  var want = true
  var error = null
  var ended = false
  var running = false

  stream.on('readable', onreadable)
  onreadable()

  if (cb) eos(stream, {readable: true, writable: false}, done)
  return stream

  function done (err) {
    if (!error) error = err
    ended = true
    if (!running) cb(error)
  }

  function onreadable () {
    if (want) read()
  }

  function afterRead (err) {
    running = false

    if (err) {
      error = err
      if (ended) return cb(error)
      stream.destroy(err)
      return
    }
    if (ended) return cb(error)
    read()
  }

  function read () {
    if (ended || running) return
    want = false

    var data = stream.read()
    if (!data) {
      want = true
      return
    }

    running = true
    fn(data, afterRead)
  }
}
