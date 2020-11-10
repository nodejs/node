'use strict'

const cacache = require('cacache')
const Fetcher = require('../../fetch')
const regManifest = require('./manifest')
const regPackument = require('./packument')
const regTarball = require('./tarball')

const fetchRegistry = module.exports = Object.create(null)

Fetcher.impl(fetchRegistry, {
  packument (spec, opts) {
    return regPackument(spec, opts)
  },

  manifest (spec, opts) {
    return regManifest(spec, opts)
  },

  tarball (spec, opts) {
    return regTarball(spec, opts)
  },

  fromManifest (manifest, spec, opts) {
    return regTarball.fromManifest(manifest, spec, opts)
  },

  clearMemoized () {
    cacache.clearMemoized()
    regPackument.clearMemoized()
  }
})
