var test = require('tap').test
var path = require('path')
var common = require('../common-tap.js')
var mr = common.fakeRegistry.compat
var server

var packageName = path.basename(__filename, '.js')

test('setup', function (t) {
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s
    t.end()
  })
})

test('package names not mangled on error with non-root registry', function (t) {
  server.get('/' + packageName).reply(404, {})
  common.npm(
    [
      '--registry=' + common.registry,
      '--cache=' + common.cache,
      'cache',
      'add',
      packageName + '@*'
    ],
    {},
    function (er, code, stdout, stderr) {
      t.ifError(er, 'correctly handled 404')
      t.equal(code, 1, 'exited with error')
      t.match(stderr, packageName, 'should have package name in error')
      server.done()
      server.close()
      t.end()
    }
  )
})
