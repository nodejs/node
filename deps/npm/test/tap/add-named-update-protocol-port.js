'use strict'
var path = require('path')
require('../common-tap')
var nock = require('nock')
var test = require('tap').test
var npm = require('../../')
var addNamed = require('../../lib/cache/add-named')

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

test('tarball paths should update port if updating protocol', function (t) {
  nock('http://localhost:1337/registry')
    .get('/' + packageName)
    .reply(200, fooPkg)

  nock('http://localhost:1337/registry')
    .get('/' + packageName + '/-/' + packageName + '-0.0.0.tgz')
    .reply(200, '1')

  nock('http://localhost:1338/registry')
    .get('/' + packageName + '/-/' + packageName + '-0.0.0.tgz')
    .reply(404)

  npm.load({registry: 'http://localhost:1337/registry', global: true}, function () {
    addNamed(packageName, '0.0.0', null, function checkPath (err, pkg) {
      t.ifError(err, 'addNamed worked')
      t.end()
    })
  })
})

test('tarball paths should NOT update if different hostname', function (t) {
  nock('http://localhost:1337/registry')
    .get('/' + iPackageName)
    .reply(200, fooiPkg)

  nock('http://127.0.0.1:1338/registry')
    .get('/' + iPackageName + '/-/' + iPackageName + '-0.0.0.tgz')
    .reply(200, '1')

  nock('http://127.0.0.1:1337/registry')
    .get('/' + iPackageName + '/-/' + iPackageName + '-0.0.0.tgz')
    .reply(404)

  npm.load({registry: 'http://localhost:1337/registry', global: true}, function () {
    addNamed(iPackageName, '0.0.0', null, function checkPath (err, pkg) {
      t.ifError(err, 'addNamed worked')
      t.end()
    })
  })
})
