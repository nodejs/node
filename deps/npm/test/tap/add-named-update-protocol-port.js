'use strict'
var path = require('path')
var test = require('tap').test
var common = require('../common-tap')
var mr = require('npm-registry-mock')
var server1
var server2

var packageName = path.basename(__filename, '.js')

var fooPkg = {
  name: packageName,
  versions: {
    '0.0.0': {
      name: packageName,
      version: '0.0.0',
      dist: {
        tarball: 'https://localhost:1338/registry/' + packageName + '/-/' + packageName + '-0.0.0.tgz',
        shasum: '356a192b7913b04c54574d18c28d46e6395428ab'
      }
    }
  }
}

var iPackageName = packageName + 'i'
var fooiPkg = {
  name: iPackageName,
  versions: {
    '0.0.0': {
      name: iPackageName,
      version: '0.0.0',
      dist: {
        tarball: 'http://127.0.0.1:1338/registry/' + iPackageName + '/-/' + iPackageName + '-0.0.0.tgz',
        shasum: '356a192b7913b04c54574d18c28d46e6395428ab'
      }
    }
  }
}

test('setup', function (t) {
  mr({
    port: 1337,
    throwOnUnmatched: true
  }, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server1 = s
    mr({
      port: 1338,
      throwOnUnmatched: true
    }, function (err, s) {
      t.ifError(err, 'registry mocked successfully')
      server2 = s
      t.end()
    })
  })
})

test('tarball paths should update port if updating protocol', function (t) {
  server1.get('/registry/' + packageName).reply(200, fooPkg)
  server1.get(
    '/registry/' + packageName + '/-/' + packageName + '-0.0.0.tgz'
  ).reply(200, '1')

  common.npm(
    [
      'cache',
      'add',
      packageName + '@0.0.0',
      '--registry',
      'http://localhost:1337/registry'
    ],
    {},
    function (er, code, stdout, stderr) {
      if (er) { throw er }
      t.equal(code, 0, 'addNamed worked')
      server1.done()
      t.end()
    }
  )
})

test('tarball paths should NOT update if different hostname', function (t) {
  server1.get('/registry/' + iPackageName).reply(200, fooiPkg)
  server2.get(
    '/registry/' + iPackageName + '/-/' + iPackageName + '-0.0.0.tgz'
  ).reply(200, '1')

  common.npm(
    [
      'cache',
      'add',
      iPackageName + '@0.0.0',
      '--registry',
      'http://localhost:1337/registry'
    ],
    {},
    function (er, code, stdout, stderr) {
      if (er) { throw er }
      t.equal(code, 0, 'addNamed worked')
      server1.done()
      server2.done()
      t.end()
    }
  )
})

test('cleanup', function (t) {
  t.pass('cleaned up')
  server1.close()
  server2.close()
  t.end()
})
