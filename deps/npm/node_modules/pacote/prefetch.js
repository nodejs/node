'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const finished = BB.promisify(require('mississippi').finished)
const optCheck = require('./lib/util/opt-check')
const npa = require('npm-package-arg')

module.exports = prefetch
function prefetch (spec, opts) {
  opts = optCheck(opts)
  spec = npa(spec, opts.where)
  opts.log.warn('prefetch', 'pacote.prefetch() is deprecated. Please use pacote.tarball() instead.')
  const startTime = Date.now()
  if (!opts.cache) {
    opts.log.info('prefetch', 'skipping prefetch: no cache provided')
    return BB.resolve({spec})
  }
  if (opts.integrity && !opts.preferOnline) {
    opts.log.silly('prefetch', 'checking if', opts.integrity, 'is already cached')
    return cacache.get.hasContent(opts.cache, opts.integrity).then(info => {
      if (info) {
        opts.log.silly('prefetch', `content already exists for ${spec} (${Date.now() - startTime}ms)`)
        return {
          spec,
          integrity: info.integrity,
          size: info.size,
          byDigest: true
        }
      } else {
        return prefetchByManifest(startTime, spec, opts)
      }
    })
  } else {
    opts.log.silly('prefetch', `no integrity hash provided for ${spec} - fetching by manifest`)
    return prefetchByManifest(startTime, spec, opts)
  }
}

let fetch
function prefetchByManifest (start, spec, opts) {
  let manifest
  let integrity
  return BB.resolve().then(() => {
    if (!fetch) {
      fetch = require('./lib/fetch')
    }
    const stream = fetch.tarball(spec, opts)
    if (!stream) { return }
    stream.on('data', function () {})
    stream.on('manifest', m => { manifest = m })
    stream.on('integrity', i => { integrity = i })
    return finished(stream)
  }).then(() => {
    opts.log.silly('prefetch', `${spec} done in ${Date.now() - start}ms`)
    return {
      manifest,
      spec,
      integrity: integrity || (manifest && manifest._integrity),
      byDigest: false
    }
  })
}
