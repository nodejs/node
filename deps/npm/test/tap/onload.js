var path = require('path')
var test = require('tap').test
var rimraf = require('rimraf')
var common = require('../common-tap.js')
var opts = { cwd: common.pkg }
var binDir = '../../node_modules/.bin'
var fixture = path.resolve(__dirname, binDir)
var onload = path.resolve(__dirname, '../fixtures/onload.js')

test('setup', function (t) {
  rimraf.sync(path.join(common.pkg, 'node_modules'))
  t.end()
})

test('npm bin with valid onload script', function (t) {
  var args = ['--onload', onload, 'bin']
  common.npm(args, opts, function (err, code, stdout, stderr) {
    t.ifError(err, 'bin ran without issue')
    t.equal(stderr.trim(), 'called onload')
    t.equal(code, 0, 'exit ok')
    t.equal(stdout, fixture + '\n')
    t.end()
  })
})

test('npm bin with invalid onload script', function (t) {
  var onloadScript = onload + 'jsfd'
  var args = ['--onload', onloadScript, '--loglevel=warn', 'bin']
  common.npm(args, opts, function (err, code, stdout, stderr) {
    t.ifError(err, 'bin ran without issue')
    t.match(stderr, /npm WARN onload-script failed to require onload script/)
    t.match(stderr, /MODULE_NOT_FOUND/)
    t.notEqual(stderr.indexOf(onloadScript), -1)
    t.equal(code, 0, 'exit ok')
    var res = path.resolve(stdout)
    t.equal(res, fixture + '\n')
    t.end()
  })
})
