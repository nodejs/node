'use strict'

const duck = require('protoduck')

const Fetcher = duck.define(['spec', 'opts', 'manifest'], {
  manifest: ['spec', 'opts'],
  tarball: ['spec', 'opts'],
  fromManifest: ['manifest', 'spec', 'opts'],
  clearMemoized () {}
}, {name: 'Fetcher'})
module.exports = Fetcher

module.exports.manifest = manifest
function manifest (spec, opts) {
  const fetcher = getFetcher(spec.type)
  return fetcher.manifest(spec, opts)
}

module.exports.tarball = tarball
function tarball (spec, opts) {
  return getFetcher(spec.type).tarball(spec, opts)
}

module.exports.fromManifest = fromManifest
function fromManifest (manifest, spec, opts) {
  return getFetcher(spec.type).fromManifest(manifest, spec, opts)
}

const fetchers = {}

module.exports.clearMemoized = clearMemoized
function clearMemoized () {
  Object.keys(fetchers).forEach(k => {
    fetchers[k].clearMemoized()
  })
}

function getFetcher (type) {
  if (!fetchers[type]) {
    // This is spelled out both to prevent sketchy stuff and to make life
    // easier for bundlers/preprocessors.
    switch (type) {
      case 'directory':
        fetchers[type] = require('./fetchers/directory')
        break
      case 'file':
        fetchers[type] = require('./fetchers/file')
        break
      case 'git':
        fetchers[type] = require('./fetchers/git')
        break
      case 'hosted':
        fetchers[type] = require('./fetchers/hosted')
        break
      case 'range':
        fetchers[type] = require('./fetchers/range')
        break
      case 'remote':
        fetchers[type] = require('./fetchers/remote')
        break
      case 'tag':
        fetchers[type] = require('./fetchers/tag')
        break
      case 'version':
        fetchers[type] = require('./fetchers/version')
        break
      default:
        throw new Error(`Invalid dependency type requested: ${type}`)
    }
  }
  return fetchers[type]
}
