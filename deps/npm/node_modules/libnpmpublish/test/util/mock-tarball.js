'use strict'

const BB = require('bluebird')

const getStream = require('get-stream')
const tar = require('tar-stream')
const zlib = require('zlib')

module.exports = makeTarball
function makeTarball (files, opts) {
  opts = opts || {}
  const pack = tar.pack()
  Object.keys(files).forEach(function (filename) {
    const entry = files[filename]
    pack.entry({
      name: (opts.noPrefix ? '' : 'package/') + filename,
      type: entry.type,
      size: entry.size,
      mode: entry.mode,
      mtime: entry.mtime || new Date(0),
      linkname: entry.linkname,
      uid: entry.uid,
      gid: entry.gid,
      uname: entry.uname,
      gname: entry.gname
    }, typeof files[filename] === 'string'
      ? files[filename]
      : files[filename].data)
  })
  pack.finalize()
  return BB.try(() => {
    if (opts.stream && opts.gzip) {
      const gz = zlib.createGzip()
      pack.on('error', err => gz.emit('error', err)).pipe(gz)
    } else if (opts.stream) {
      return pack
    } else {
      return getStream.buffer(pack).then(ret => {
        if (opts.gzip) {
          return BB.fromNode(cb => zlib.gzip(ret, cb))
        } else {
          return ret
        }
      })
    }
  })
}
