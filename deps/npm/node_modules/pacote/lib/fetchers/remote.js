'use strict'

const BB = require('bluebird')

const Fetcher = require('../fetch')
const fetchRegistry = require('./registry')

const fetchRemote = module.exports = Object.create(null)

Fetcher.impl(fetchRemote, {
  packument (spec, opts) {
    return BB.reject(new Error('Not implemented yet'))
  },

  manifest (spec, opts) {
    // We can't get the manifest for a remote tarball until
    // we extract the tarball itself.
    // `finalize-manifest` takes care of this process of extracting
    // a manifest based on ./tarball.js
    return BB.resolve(null)
  },

  tarball (spec, opts) {
    const uri = spec._resolved || spec.fetchSpec
    return fetchRegistry.fromManifest({
      _resolved: uri,
      _integrity: opts.integrity
    }, spec, opts)
  },

  fromManifest (manifest, spec, opts) {
    return this.tarball(manifest || spec, opts)
  }
})
