'use strict'

const pkgsri = require('../../lib/utils/package-integrity.js')
const ssri = require('ssri')
const test = require('tap').test

test('generates integrity according to spec', (t) => {
  const pkgJson = {
    'name': 'foo',
    'version': '1.0.0',
    'dependencies': {
      'x': '1.0.0'
    },
    'devDependencies': {
      'y': '1.0.0'
    },
    'optionalDependencies': {
      'z': '1.0.0'
    }
  }
  const integrity = pkgsri.hash(pkgJson)
  t.ok(integrity && integrity.toString(), 'hash returned')
  t.equal(
    ssri.parse(integrity).toString(),
    integrity,
    'hash is a valid ssri string'
  )
  t.ok(pkgsri.check(pkgJson, integrity), 'same-data integrity check succeeds')
  t.done()
})

test('updates if anything changes in package.json', (t) => {
  const pkgJson = {
    'name': 'foo',
    'version': '1.0.0',
    'dependencies': {
      'x': '1.0.0'
    },
    'devDependencies': {
      'y': '1.0.0'
    },
    'optionalDependencies': {
      'z': '1.0.0'
    }
  }
  const sri = pkgsri.hash(pkgJson)
  pkgJson.version = '1.2.3'
  t.equal(pkgsri.check(pkgJson, sri), false, 'no match after pkgJson change')
  t.done()
})
