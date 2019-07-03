'use strict'
var path = require('path')
var test = require('tap').test
var mr = require('npm-registry-mock')
var common = require('../common-tap')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

var testdir = common.pkg
var cachedir = path.join(testdir, 'cache')

var fixture = new Tacks(Dir({
  cache: Dir(),
  node_modules: Dir(),
  tarballs: Dir({
    'pkgA.tgz': File(Buffer.from(
      '1f8b0800000000000003edcfcf0a0221100670cf3ec5e0396cfcb703bd8d' +
      '842cb5e4ca5a5da2776f5da153b78408fc5d3e6684e133f9e3e4c7b04f35' +
      'e539cf9135868883b5509206b725ea3a6f9c01a634598d8e48134365d0e0' +
      'fadebac827b77cf5cb5ae5db3bf52bf0ce3ff1e00022fa4b100710691abd' +
      'd895cd3d2cf934c7b25412250afee4bfaeda755dd735f40211b5bced0008' +
      '0000',
      'hex'
    )),
    'pkgB1.tgz': File(Buffer.from(
      '1f8b0800000000000003edcfc10a0221140550d77ec5c375d8d3d111fa1b' +
      '0b196ac891b16913fd7be308adda2544f0cee6e25d3caec99f463f847daa' +
      '292f798aac3144ec8d8192aeb75ba2aeef8ded8029ed8c46eb1c1a86aa43' +
      'bd76d87ac8274bbef9799df2ed9dfa1578e79f78700011fd35880388340e' +
      '47b12bcd3dccf93cc5522a8912057ff25f4f258410d2d00b247d22080008' +
      '0000',
      'hex'
    ))
  })
}))

var pkgAtgz = path.join(testdir, 'tarballs', 'pkgA.tgz')
var pkgB1tgz = path.join(testdir, 'tarballs', 'pkgB1.tgz')

var server

var pkgA = {
  name: 'pkg-a',
  'dist-tags': {
    latest: '1.0.0'
  },
  versions: {
    '1.0.0': {
      name: 'pkg-a',
      version: '1.0.0',
      dependencies: {
        'pkg-b': '1.0.0'
      },
      dist: {
        shasum: 'dc5471ce0439f0f47749bb01473cad4570cc7dc5',
        tarball: common.registry + '/pkg-a/-/pkg-a-1.0.0.tgz'
      }
    }
  }
}

var pkgB = {
  name: 'pkg-b',
  'dist-tags': {
    latest: '1.0.0'
  },
  versions: {
    '1.0.0': {
      name: 'pkg-b',
      version: '1.0.0',
      dist: {
        shasum: '53031aa2cf774c0e841c6fdbbe54c13825cd5640',
        tarball: common.registry + '/pkg-b/-/pkg-b-1.0.0.tgz'
      }
    },
    '1.0.0rc1': {
      name: 'pkg-b',
      version: '1.0.0rc1',
      dist: {
        shasum: '7f4b1bf680e3a31113d77619b4dc7c3b4c7dc15c',
        tarball: common.registry + '/pkg-b/-/pkg-b-1.0.0-rc1.tgz'
      }
    }
  }
}

function setup () {
  cleanup()
  fixture.create(testdir)
}
function cleanup () {
  fixture.remove(testdir)
}

test('setup', function (t) {
  setup()
  mr({ port: common.port, throwOnUnmatched: true }, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s
    t.end()
  })
})

test('invalid versions should be ignored', function (t) {
  server.get('/pkg-a').reply(200, pkgA)
  server.get('/pkg-b').reply(200, pkgB)
  server.get('/pkg-a/-/pkg-a-1.0.0.tgz').replyWithFile(200, pkgAtgz)
  server.get('/pkg-b/-/pkg-b-1.0.0.tgz').replyWithFile(200, pkgB1tgz)

  common.npm(
    [
      'install',
      '--cache', cachedir,
      '--registry', common.registry,
      '--fetch-retries=0',
      'pkg-a@1.0.0'
    ],
    {cwd: testdir},
    function (err, code, stdout, stderr) {
      if (err) throw err
      t.equal(code, 0, 'install succeded')
      t.comment(stdout.trim())
      t.comment(stderr.trim())
      server.done()
      t.end()
    }
  )
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.end()
})
