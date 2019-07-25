var test = require('tap').test
var common = require('../common-tap.js')
var mr = common.fakeRegistry.compat
var server

test('setup', function (t) {
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s
    t.end()
  })
})

test('scoped package names not mangled on error with non-root registry', function (t) {
  server.get('/@scope%2ffoo').reply(404, {})
  common.npm(
    [
      '--registry=' + common.registry,
      '--cache=' + common.cache,
      'cache',
      'add',
      '@scope/foo@*',
      '--force'
    ],
    {},
    function (er, code, stdout, stderr) {
      t.ifError(er, 'correctly handled 404')
      t.equal(code, 1, 'exited with error')
      t.match(stderr, /E404/, 'should notify the sort of error as a 404')
      t.match(stderr, /@scope(?:%2f|\/)foo/, 'should have package name in error')
      server.done()
      server.close()
      t.end()
    }
  )
})
