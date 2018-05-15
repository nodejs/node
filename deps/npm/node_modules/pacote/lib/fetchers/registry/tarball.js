'use strict'

const BB = require('bluebird')

const fetch = require('./fetch')
const manifest = require('./manifest')
const optCheck = require('../../util/opt-check')
const PassThrough = require('stream').PassThrough
const pickRegistry = require('./pick-registry')
const ssri = require('ssri')
const url = require('url')

module.exports = tarball
function tarball (spec, opts) {
  opts = optCheck(opts)
  const registry = pickRegistry(spec, opts)
  const stream = new PassThrough()
  let mani
  if (
    opts.resolved &&
    // spec.type === 'version' &&
    opts.resolved.indexOf(registry) === 0
  ) {
    // fakeChild is a shortcut to avoid looking up a manifest!
    mani = BB.resolve({
      name: spec.name,
      version: spec.fetchSpec,
      _integrity: opts.integrity,
      _resolved: opts.resolved,
      _fakeChild: true
    })
  } else {
    // We can't trust opts.resolved if it's going to a separate host.
    mani = manifest(spec, opts)
  }

  mani.then(mani => {
    !mani._fakeChild && stream.emit('manifest', mani)
    const fetchStream = fromManifest(mani, spec, opts).on(
      'integrity', i => stream.emit('integrity', i)
    )
    fetchStream.on('error', err => stream.emit('error', err))
    fetchStream.pipe(stream)
  }).catch(err => stream.emit('error', err))
  return stream
}

module.exports.fromManifest = fromManifest
function fromManifest (manifest, spec, opts) {
  opts = optCheck(opts)
  opts.scope = spec.scope || opts.scope
  const stream = new PassThrough()
  const registry = pickRegistry(spec, opts)
  const uri = getTarballUrl(spec, registry, manifest, opts)
  fetch(uri, registry, Object.assign({
    headers: {
      'pacote-req-type': 'tarball',
      'pacote-pkg-id': `registry:${manifest.name}@${uri}`
    },
    integrity: manifest._integrity,
    algorithms: [
      manifest._integrity
        ? ssri.parse(manifest._integrity).pickAlgorithm()
        : 'sha1'
    ],
    spec
  }, opts))
    .then(res => {
      const hash = res.headers.get('x-local-cache-hash')
      if (hash) {
        stream.emit('integrity', decodeURIComponent(hash))
      }
      res.body.on('error', err => stream.emit('error', err))
      res.body.pipe(stream)
    })
    .catch(err => stream.emit('error', err))
  return stream
}

function getTarballUrl (spec, registry, mani, opts) {
  const reg = url.parse(registry)
  const tarball = url.parse(mani._resolved)
  // https://github.com/npm/npm/pull/9471
  //
  // TL;DR: Some alternative registries host tarballs on http and packuments
  // on https, and vice-versa. There's also a case where people who can't use
  // SSL to access the npm registry, for example, might use
  // `--registry=http://registry.npmjs.org/`. In this case, we need to
  // rewrite `tarball` to match the protocol.
  //
  if (reg.hostname === tarball.hostname && reg.protocol !== tarball.protocol) {
    tarball.protocol = reg.protocol
    // Ports might be same host different protocol!
    if (reg.port !== tarball.port) {
      delete tarball.host
      tarball.port = reg.port
    }
    delete tarball.href
  }
  return url.format(tarball)
}
