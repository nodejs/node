var path = require('path')
var test = require('tap').test
var rimraf = require('rimraf')
var common = require('../common-tap.js')
var opts = { cwd: __dirname }
var binDir = '../../node_modules/.bin'
var fixture = path.resolve(__dirname, binDir)

test('setup', function (t) {
  rimraf.sync(path.join(__dirname, 'node_modules'))
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
