'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const cacheKey = require('./cache-key')
const optCheck = require('./opt-check')
const pipe = BB.promisify(require('mississippi').pipe)
const tar = require('tar-fs')

module.exports = packDir
function packDir (manifest, label, dir, target, opts) {
  opts = optCheck(opts)

  const packer = opts.dirPacker
  ? opts.dirPacker(manifest, dir)
  : tar.pack(dir, {
    map: header => {
      header.name = 'package/' + header.name
      header.mtime = 0 // make tarballs idempotent
      return header
    },
    ignore: (name) => {
      return name.match(/\.git/)
    }
  })

  if (!opts.cache) {
    return pipe(packer, target).catch(err => {
      throw err
    })
  } else {
    const cacher = cacache.put.stream(
      opts.cache, cacheKey('packed-dir', label), opts
    ).on('integrity', i => {
      target.emit('integrity', i)
    })
    return BB.all([
      pipe(packer, cacher),
      pipe(packer, target)
    ])
  }
}
