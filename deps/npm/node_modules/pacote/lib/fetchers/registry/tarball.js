'use strict'

const BB = require('bluebird')

const fetch = require('./fetch')
const manifest = require('./manifest')
const optCheck = require('../../util/opt-check')
const PassThrough = require('stream').PassThrough
const pickRegistry = require('./pick-registry')
const pipe = BB.promisify(require('mississippi').pipe)
const ssri = require('ssri')
const url = require('url')

module.exports = tarball
function tarball (spec, opts) {
  opts = optCheck(opts)
  const stream = new PassThrough()
  manifest(spec, opts).then(manifest => {
    stream.emit('manifest', manifest)
    return pipe(
      fromManifest(manifest, spec, opts).on(
        'integrity', i => stream.emit('integrity', i)
      ),
      stream
    )
  }).catch(err => stream.emit('error', err))
  return stream
}

module.exports.fromManifest = fromManifest
function fromManifest (manifest, spec, opts) {
  opts = optCheck(opts)
  opts.scope = spec.scope || opts.scope
  const stream = new PassThrough()
  const registry = pickRegistry(spec, opts)
  const uri = getTarballUrl(registry, manifest)
  fetch(uri, registry, Object.assign({
    headers: {
      'pacote-req-type': 'tarball',
      'pacote-pkg-id': `registry:${
        spec.type === 'remote'
        ? spec
        : `${manifest.name}@${manifest.version}`
      }`
    },
    integrity: manifest._integrity,
    algorithms: [
      manifest._integrity
      ? ssri.parse(manifest._integrity).pickAlgorithm()
      : 'sha1'
    ],
    spec
  }, opts)).then(res => {
    stream.emit('integrity', res.headers.get('x-local-cache-hash'))
    res.body.on('error', err => stream.emit('error', err))
    res.body.pipe(stream)
  }).catch(err => stream.emit('error', err))
  return stream
}

function getTarballUrl (registry, manifest) {
  // https://github.com/npm/npm/pull/9471
  //
  // TL;DR: Some alternative registries host tarballs on http and packuments on
  // https, and vice-versa. There's also a case where people who can't use SSL
  // to access the npm registry, for example, might use
  // `--registry=http://registry.npmjs.org/`. In this case, we need to rewrite
  // `tarball` to match the protocol.
  //
  const reg = url.parse(registry)
  const tarball = url.parse(manifest._resolved)
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
