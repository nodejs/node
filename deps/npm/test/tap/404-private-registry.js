require('../common-tap')
var nock = require('nock')
var test = require('tap').test
var path = require('path')
var npm = require('../../')
var addNamed = require('../../lib/cache/add-named')

var packageName = path.basename(__filename, '.js')

test('package names not mangled on error with non-root registry', function test404 (t) {
  nock('http://localhost:1337')
    .get('/registry/' + packageName)
    .reply(404, {
      error: 'not_found',
      reason: 'document not found'
    })

  npm.load({registry: 'http://localhost:1337/registry', global: true}, function () {
    addNamed(packageName, '*', null, function checkError (err) {
      t.ok(err, 'should error')
      t.equal(err.message, '404 Not Found: ' + packageName, 'should have package name in error')
      t.equal(err.pkgid, packageName, 'err.pkgid should match package name')
      t.end()
    })
  })
})
