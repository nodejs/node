var path = require('path')
var test = require('tap').test
var rimraf = require('rimraf')
var common = require('../common-tap.js')
var opts = { cwd: common.pkg }
var binDir = '../../../node_modules/.bin'
var fixture = path.resolve(common.pkg, binDir)

test('setup', function (t) {
  rimraf.sync(path.join(common.pkg, 'node_modules'))
  t.end()
})

test('npm bin', function (t) {
  common.npm(['bin'], opts, function (err, code, stdout, stderr) {
    t.ifError(err, 'bin ran without issue')
    t.notOk(stderr, 'should have no stderr')
    t.equal(code, 0, 'exit ok')
    var res = path.resolve(stdout)
    t.equal(res, fixture + '\n')
    t.end()
  })
})
