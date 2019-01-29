'use strict'

const Fetcher = require('../fetch')
const fetchRegistry = require('./registry')

const fetchRemote = module.exports = Object.create(null)

Fetcher.impl(fetchRemote, {
  packument (spec, opts) {
    return fetchRegistry.packument(spec.subSpec, opts)
  },

  manifest (spec, opts) {
    return fetchRegistry.manifest(spec.subSpec, opts)
  },

  tarball (spec, opts) {
    return fetchRegistry.tarball(spec.subSpec, opts)
  },

  fromManifest (manifest, spec, opts) {
    return fetchRegistry.fromManifest(manifest, spec.subSpec, opts)
  }
})
