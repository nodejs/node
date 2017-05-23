'use strict'

const BB = require('bluebird')

const fetch = require('./fetch')
const LRU = require('lru-cache')
const optCheck = require('../../util/opt-check')
const pickManifest = require('npm-pick-manifest')
const pickRegistry = require('./pick-registry')
const ssri = require('ssri')
const url = require('url')

// Corgis are cute. 🐕🐶
const CORGI_DOC = 'application/vnd.npm.install-v1+json; q=1.0, application/json; q=0.8, */*'
const JSON_DOC = 'application/json'

module.exports = manifest
function manifest (spec, opts) {
  opts = optCheck(opts)

  const registry = pickRegistry(spec, opts)
  const uri = metadataUrl(registry, spec.escapedName)

  return getManifest(uri, registry, spec, opts).then(manifest => {
    return annotateManifest(uri, registry, manifest)
  })
}

function metadataUrl (registry, name) {
  const normalized = registry.slice(-1) !== '/'
  ? registry + '/'
  : registry
  return url.resolve(normalized, name)
}

function getManifest (uri, registry, spec, opts) {
  return fetchPackument(uri, spec, registry, opts).then(packument => {
    try {
      return pickManifest(packument, spec.fetchSpec, {
        defaultTag: opts.defaultTag
      })
    } catch (err) {
      if (err.code === 'ETARGET' && packument._cached && !opts.offline) {
        opts.log.silly(
          'registry:manifest',
          `no matching version for ${spec.name}@${spec.fetchSpec} in the cache. Forcing revalidation`
        )
        opts.preferOffline = false
        opts.preferOnline = true
        return fetchPackument(uri, spec, registry, opts).then(packument => {
          return pickManifest(packument, spec.fetchSpec, {
            defaultTag: opts.defaultTag
          })
        })
      } else {
        throw err
      }
    }
  })
}

// TODO - make this an opt
const MEMO = new LRU({
  length: m => m._contentLength,
  max: 200 * 1024 * 1024, // 200MB
  maxAge: 30 * 1000 // 30s
})

module.exports.clearMemoized = clearMemoized
function clearMemoized () {
  MEMO.reset()
}

function fetchPackument (uri, spec, registry, opts) {
  const mem = pickMem(opts)
  if (mem && !opts.preferOnline && mem.has(uri)) {
    return BB.resolve(mem.get(uri))
  }

  return fetch(uri, registry, Object.assign({
    headers: {
      'pacote-req-type': 'packument',
      'pacote-pkg-id': `registry:${manifest.name}`,
      accept: opts.fullMetadata ? JSON_DOC : CORGI_DOC
    },
    spec
  }, opts, {
    // Force integrity to null: we never check integrity hashes for manifests
    integrity: null
  })).then(res => res.json().then(packument => {
    packument._cached = res.headers.has('x-local-cache')
    packument._contentLength = +res.headers.get('content-length')
    // NOTE - we need to call pickMem again because proxy
    //        objects get reused!
    const mem = pickMem(opts)
    if (mem) {
      mem.set(uri, packument)
    }
    return packument
  }))
}

class ObjProxy {
  get (key) { return this.obj[key] }
  set (key, val) { this.obj[key] = val }
}

// This object is used synchronously and immediately, so
// we can safely reuse it instead of consing up new ones
const PROX = new ObjProxy()
function pickMem (opts) {
  if (!opts || !opts.memoize) {
    return MEMO
  } else if (opts.memoize.get && opts.memoize.set) {
    return opts.memoize
  } else if (typeof opts.memoize === 'object') {
    PROX.obj = opts.memoize
    return PROX
  } else {
    return null
  }
}

function annotateManifest (uri, registry, manifest) {
  const shasum = manifest.dist && manifest.dist.shasum
  manifest._integrity = manifest.dist && manifest.dist.integrity
  manifest._shasum = shasum
  if (!manifest._integrity && shasum) {
    // Use legacy dist.shasum field if available.
    manifest._integrity = ssri.fromHex(shasum, 'sha1').toString()
  }
  manifest._resolved = (
    manifest.dist && manifest.dist.tarball
  )
  if (!manifest._resolved) {
    const err = new Error(
      `Manifest for ${manifest.name}@${manifest.version} from ${uri} is missing a tarball url (pkg.dist.tarball). Guessing a default.`
    )
    err.code = 'ENOTARBALL'
    err.manifest = manifest
    if (!manifest._warnings) { manifest._warnings = [] }
    manifest._warnings.push(err.message)
    manifest._resolved =
    `${registry}/${manifest.name}/-/${manifest.name}-${manifest.version}.tgz`
  }
  return manifest
}
