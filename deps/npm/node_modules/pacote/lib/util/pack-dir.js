'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const cacheKey = require('./cache-key')
const optCheck = require('./opt-check')
const packlist = require('npm-packlist')
const pipe = BB.promisify(require('mississippi').pipe)
const tar = require('tar')

module.exports = packDir
function packDir (manifest, label, dir, target, opts) {
  opts = optCheck(opts)

  const packer = opts.dirPacker
    ? BB.resolve(opts.dirPacker(manifest, dir))
    : mkPacker(dir)

  if (!opts.cache) {
    return packer.then(packer => pipe(packer, target))
  } else {
    const cacher = cacache.put.stream(
      opts.cache, cacheKey('packed-dir', label), opts
    ).on('integrity', i => {
      target.emit('integrity', i)
    })
    return packer.then(packer => BB.all([
      pipe(packer, cacher),
      pipe(packer, target)
    ]))
  }
}

function mkPacker (dir) {
  return packlist({ path: dir }).then(files => {
    return tar.c({
      cwd: dir,
      gzip: true,
      portable: true,
      prefix: 'package/'
    }, files)
  })
}
