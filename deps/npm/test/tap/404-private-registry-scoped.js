var test = require('tap').test
var common = require('../common-tap.js')
var mr = require('npm-registry-mock')
var server

test('setup', function (t) {
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s
    t.end()
  })
})

test('scoped package names not mangled on error with non-root registry', function (t) {
  common.npm(
    [
      'cache',
      'add',
      '@scope/foo@*',
      '--force'
    ],
    {},
    function (er, code, stdout, stderr) {
      t.ifError(er, 'correctly handled 404')
      t.equal(code, 1, 'exited with error')
      t.match(stderr, /404 Not found/, 'should notify the sort of error as a 404')
      t.match(stderr, /@scope\/foo/, 'should have package name in error')
      t.end()
    }
  )
})

test('cleanup', function (t) {
  t.pass('cleaned up')
  server.done()
  server.close()
  t.end()
})
