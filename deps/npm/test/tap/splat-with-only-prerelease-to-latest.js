'use strict'

const common = require('../common-tap.js')
const mr = require('npm-registry-mock')
const npm = require('../../lib/npm')
const osenv = require('osenv')
const path = require('path')
const rimraf = require('rimraf')
const test = require('tap').test

const testdir = common.pkg

const moduleName = 'xyzzy-wibble'
const testModule = {
  name: moduleName,
  'dist-tags': {
    latest: '1.3.0-a',
    other: '1.2.0-a'
  },
  versions: {
    '1.0.0-a': {
      name: moduleName,
      version: '1.0.0-a',
      dist: {
        shasum: 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
        tarball: 'http://registry.npmjs.org/aproba/-/xyzzy-wibble-1.0.0-a.tgz'
      }
    },
    '1.1.0-a': {
      name: moduleName,
      version: '1.1.0-a',
      dist: {
        shasum: 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
        tarball: 'http://registry.npmjs.org/aproba/-/xyzzy-wibble-1.1.0-a.tgz'
      }
    },
    '1.2.0-a': {
      name: moduleName,
      version: '1.2.0-a',
      dist: {
        shasum: 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
        tarball: 'http://registry.npmjs.org/aproba/-/xyzzy-wibble-1.2.0-a.tgz'
      }
    },
    '1.3.0-a': {
      name: moduleName,
      version: '1.3.0-a',
      dist: {
        shasum: 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
        tarball: 'http://registry.npmjs.org/aproba/-/xyzzy-wibble-1.3.0-a.tgz'
      }
    }
  }
}

let server
test('setup', (t) => {
  mr({port: common.port}, (er, s) => {
    if (er) throw er
    t.ok(true, 'mock registry loaded')
    server = s
    npm.load({
      loglevel: 'silent',
      registry: common.registry,
      cache: path.join(testdir, 'cache')
    }, (err) => {
      if (err) { throw err }
      t.ok(true, 'npm loaded')
      t.end()
    })
  })
})

test('splat', (t) => {
  server.get('/xyzzy-wibble').reply(200, testModule)
  return npm.commands.cache.add('xyzzy-wibble', '*', testdir).then((pkg) => {
    throw new Error(`Was not supposed to succeed on ${pkg}`)
  }).catch((err) => {
    t.equal(err.code, 'E404', 'got a 404 on the tarball fetch')
    t.equal(
      err.uri,
      testModule.versions['1.3.0-a'].dist.tarball,
      'tried to get tarball for `latest` tag'
    )
    npm.config.set('tag', 'other')
    return npm.commands.cache.add('xyzzy-wibble', '*', testdir)
  }).then((pkg) => {
    throw new Error(`Was not supposed to succeed on ${pkg}`)
  }).catch((err) => {
    t.equal(err.code, 'E404', 'got a 404 on the tarball fetch')
    t.equal(
      err.uri,
      testModule.versions['1.2.0-a'].dist.tarball,
      'tried to get tarball for `other` tag'
    )
    server.close()
  })
})

test('cleanup', (t) => {
  process.chdir(osenv.tmpdir())
  rimraf(testdir, () => {
    t.ok(true, 'cleaned up test dir')
    t.done()
  })
})
