'use strict'
var test = require('tap').test

var pickManifest = require('../../lib/utils/pick-manifest-from-registry-metadata.js')

test('basic carat range selection', function (t) {
  var metadata = {
    'dist-tags': {
      'example': '1.1.0'
    },
    versions: {
      '1.0.0': { version: '1.0.0' },
      '1.0.1': { version: '1.0.1' },
      '1.0.2': { version: '1.0.2' },
      '1.1.0': { version: '1.1.0' },
      '2.0.0': { version: '2.0.0' }
    }
  }
  var versions = Object.keys(metadata.versions)
  var selected = pickManifest('^1.0.0', 'latest', versions, metadata)
  t.equal(selected.manifest.version, '1.1.0', 'picked the right manifest using ^')
  t.equal(selected.resolvedTo, '1.1.0', 'resolved using version match')
  selected = pickManifest('^1.0.0', 'example', versions, metadata)
  t.equal(selected.manifest.version, '1.1.0', 'picked the right manifest using ^')
  t.equal(selected.resolvedTo, 'example', 'resolved using tag')
  t.end()
})

test('basic tilde range selection', function (t) {
  var metadata = {
    'dist-tags': {
      'example': '1.1.0'
    },
    versions: {
      '1.0.0': { version: '1.0.0' },
      '1.0.1': { version: '1.0.1' },
      '1.0.2': { version: '1.0.2' },
      '1.1.0': { version: '1.1.0' },
      '2.0.0': { version: '2.0.0' }
    }
  }
  var versions = Object.keys(metadata.versions)
  var selected = pickManifest('~1.0.0', 'latest', versions, metadata)
  t.equal(selected.manifest.version, '1.0.2', 'picked the right manifest using ~')
  t.equal(selected.resolvedTo, '1.0.2', 'resolved using version match')
  t.end()
})

test('basic mathematical range selection', function (t) {
  var metadata = {
    'dist-tags': {},
    versions: {
      '1.0.0': { version: '1.0.0' },
      '1.0.1': { version: '1.0.1' },
      '1.0.2': { version: '1.0.2' },
      '2.0.0': { version: '2.0.0' }
    }
  }
  var versions = Object.keys(metadata.versions)
  var selected = pickManifest('>=1.0.0 <2', 'example', versions, metadata)
  t.equal(selected.manifest.version, '1.0.2', 'picked the right manifest using mathematical range')
  t.equal(selected.resolvedTo, '1.0.2', 'resolved using version match')
  t.end()
})

test('basic version selection', function (t) {
  var metadata = {
    'dist-tags': {},
    versions: {
      '1.0.0': { version: '1.0.0' },
      '1.0.1': { version: '1.0.1' },
      '1.0.2': { version: '1.0.2' },
      '2.0.0': { version: '2.0.0' }
    }
  }
  var versions = Object.keys(metadata.versions)
  var selected = pickManifest('1.0.0', 'latest', versions, metadata)
  t.equal(selected.manifest.version, '1.0.0', 'picked the right manifest using specific version')
  t.equal(selected.resolvedTo, '1.0.0', 'resolved using version match')
  t.end()
})

test('nothing if range does not match anything', function (t) {
  var metadata = {
    'dist-tags': {},
    versions: {
      '1.0.0': { version: '1.0.0' },
      '2.0.0': { version: '2.0.0' },
      '2.0.5': { version: '2.0.5' }
    }
  }
  var versions = Object.keys(metadata.versions)
  var selected = pickManifest('^2.1.0', 'latest', versions, metadata)
  t.equal(selected, undefined, 'no manifest matched')
  t.end()
})

test('if `defaultTag` matches a given range, use it', function (t) {
  var metadata = {
    'dist-tags': {
      foo: '1.0.1'
    },
    versions: {
      '1.0.0': { version: '1.0.0' },
      '1.0.1': { version: '1.0.1' },
      '1.0.2': { version: '1.0.2' },
      '2.0.0': { version: '2.0.0' }
    }
  }
  var versions = Object.keys(metadata.versions)
  var selected = pickManifest('^1.0.0', 'foo', versions, metadata)
  t.equal(selected.manifest.version, '1.0.1', 'picked the version for foo')
  t.equal(selected.resolvedTo, 'foo', 'resolved using tag')

  selected = pickManifest('^2.0.0', 'foo', versions, metadata)
  t.equal(selected.manifest.version, '2.0.0', 'no match, no foo')
  t.equal(selected.resolvedTo, '2.0.0', 'resolved using version match')

  t.end()
})

test('* ranges use `defaultTag` if no versions match', function (t) {
  var metadata = {
    'dist-tags': {
      beta: '2.0.0-beta.0'
    },
    versions: {
      '1.0.0-pre.0': { version: '1.0.0-pre.0' },
      '1.0.0-pre.1': { version: '1.0.0-pre.1' },
      '2.0.0-beta.0': { version: '2.0.0-beta.0' },
      '2.0.0-beta.1': { version: '2.0.0-beta.1' }
    }
  }
  var versions = Object.keys(metadata.versions)
  var selected = pickManifest('*', 'beta', versions, metadata)
  t.equal(selected.manifest.version, '2.0.0-beta.0', 'used defaultTag for all-prerelease splat.')
  t.equal(selected.resolvedTo, 'beta', 'resolved using tag')
  t.end()
})

test('no result if metadata has no versions', function (t) {
  var selected = pickManifest('^1.0.0', 'latest', [], {'dist-tags': {}, versions: {}})
  t.equal(selected, undefined, 'no versions, no result')
  t.end()
})
