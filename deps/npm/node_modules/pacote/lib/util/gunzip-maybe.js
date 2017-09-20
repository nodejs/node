'use strict'

const duplex = require('mississippi').duplex
const through = require('mississippi').through
const zlib = require('zlib')

function hasGzipHeader (c) {
  return c[0] === 0x1F && c[1] === 0x8B && c[2] === 0x08
}

module.exports = gunzip
function gunzip () {
  const stream = duplex()
  const peeker = through((chunk, enc, cb) => {
    const newStream = hasGzipHeader(chunk)
    ? zlib.createGunzip()
    : through()
    stream.setReadable(newStream)
    stream.setWritable(newStream)
    stream.write(chunk)
  })
  stream.setWritable(peeker)
  return stream
}
