var fs = require('fs')
var path = require('path')
var test = require('tap').test
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'config-private')
var opts = { cwd: pkg }

test('setup', function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  t.end()
})

test('config get private var (old auth)', function (t) {
  common.npm([
      'config',
      'get',
      '_auth'
    ],
    opts,
    function (err, code, stdout, stderr) {
      t.ifError(err)

      t.similar(stderr, /sekretz/, 'password blocked on stderr')
      t.equal(stdout, '', 'no output')
      t.end()
    }
  )
})

test('config get private var (new auth)', function (t) {
  common.npm([
      'config',
      'get',
      '//registry.npmjs.org/:_password'
    ],
    opts,
    function (err, code, stdout, stderr) {
      t.ifError(err)

      t.similar(stderr, /sekretz/, 'password blocked on stderr')
      t.equal(stdout, '', 'no output')
      t.end()
    }
  )
})

test('config get public var (new username)', function (t) {
  var FIXTURE_PATH = path.resolve(pkg, 'fixture_npmrc')
  var s = '//registry.lvh.me/:username = wombat\n' +
          '//registry.lvh.me/:_password = YmFkIHBhc3N3b3Jk\n' +
          '//registry.lvh.me/:email = lindsay@wdu.org.au\n'
  fs.writeFileSync(FIXTURE_PATH, s, 'ascii')
  fs.chmodSync(FIXTURE_PATH, '0444')

  common.npm(
    [
      'config',
      'get',
      '//registry.lvh.me/:username',
      '--userconfig=' + FIXTURE_PATH,
      '--registry=http://registry.lvh.me/'
    ],
    opts,
    function (err, code, stdout, stderr) {
      t.ifError(err)

      t.equal(stderr, '', 'stderr is empty')
      t.equal(stdout, 'wombat\n', 'got usename is output')
      t.end()
    }
  )
})

test('clean', function (t) {
  rimraf.sync(pkg)
  t.end()
})
