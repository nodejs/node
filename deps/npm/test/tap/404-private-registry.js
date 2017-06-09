var test = require('tap').test
var path = require('path')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var common = require('../common-tap.js')
var mr = require('npm-registry-mock')
var server

var packageName = path.basename(__filename, '.js')
var testdir = path.join(__dirname, packageName)

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

test('package names not mangled on error with non-root registry', function (t) {
  server.get('/' + packageName).reply(404, {})
  common.npm(
    [
      '--registry=' + common.registry,
      '--cache=' + testdir,
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
