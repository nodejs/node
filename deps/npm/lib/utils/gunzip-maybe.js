var duplex = require('mississippi').duplex
var through = require('mississippi').through
var zlib = require('zlib')

function hasGzipHeader (c) {
  return c[0] === 0x1F && c[1] === 0x8B && c[2] === 0x08
}

module.exports = gunzip
function gunzip () {
  var stream = duplex()
  var peeker = through(function (chunk, enc, cb) {
    var newStream = hasGzipHeader(chunk)
    ? zlib.createGunzip()
    : through()
    stream.setReadable(newStream)
    stream.setWritable(newStream)
    stream.write(chunk)
  })
  stream.setWritable(peeker)
  return stream
}
