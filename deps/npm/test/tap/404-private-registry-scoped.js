var test = require('tap').test
var path = require('path')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var common = require('../common-tap.js')
var mr = common.fakeRegistry.compat
var server

var testdir = path.join(__dirname, path.basename(__filename, '.js'))

function setup () {
  cleanup()
  mkdirp.sync(testdir)
}

function cleanup () {
  rimraf.sync(testdir)
}

test('setup', function (t) {
  setup()
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
      '--cache=' + testdir,
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
      t.end()
    }
  )
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.pass('cleaned up')
  t.end()
})
