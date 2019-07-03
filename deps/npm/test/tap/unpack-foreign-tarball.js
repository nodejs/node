var fs = require('graceful-fs')
var path = require('path')

var test = require('tap').test
var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')

var common = require('../common-tap.js')

var fixtures = path.resolve(__dirname, '..', 'fixtures')

var pkg = common.pkg
var nm = path.resolve(pkg, 'node_modules')
var target = path.resolve(nm, 'npm-test-gitignore')
var cache = path.resolve(pkg, 'cache')
var tmp = path.resolve(pkg, 'tmp')

var EXEC_OPTS = {
  env: {
    'npm_config_cache': cache,
    'npm_config_tmp': tmp
  },
  cwd: pkg
}

function verify (t, files, err, code) {
  if (code) {
    t.fail('exited with failure: ' + code)
    return t.end()
  }
  var actual = fs.readdirSync(target).sort()
  var expect = files.concat(['.npmignore', 'package.json']).sort()
  t.same(actual, expect)
  t.end()
}

test('setup', function (t) {
  setup()
  t.comment('test for https://github.com/npm/npm/issues/5658')
  t.end()
})

test('npmignore only', function (t) {
  var file = path.resolve(fixtures, 'npmignore.tgz')
  common.npm(['install', file], EXEC_OPTS, verify.bind(null, t, ['foo']))
})

test('gitignore only', function (t) {
  setup()
  var file = path.resolve(fixtures, 'gitignore.tgz')
  common.npm(['install', file], EXEC_OPTS, verify.bind(null, t, ['foo']))
})

test('gitignore and npmignore', function (t) {
  setup()
  var file = path.resolve(fixtures, 'gitignore-and-npmignore.tgz')
  common.npm(['install', file], EXEC_OPTS, verify.bind(null, t, ['foo', 'bar']))
})

test('gitignore and npmignore, not gzipped 1/2', function (t) {
  setup()
  var file = path.resolve(fixtures, 'gitignore-and-npmignore.tar')
  common.npm(['install', file], EXEC_OPTS, verify.bind(null, t, ['foo', 'bar']))
})

test('gitignore and npmignore, not gzipped 2/2', function (t) {
  setup()
  var file = path.resolve(fixtures, 'gitignore-and-npmignore-2.tar')
  common.npm(['install', file], EXEC_OPTS, verify.bind(null, t, ['foo', 'bar']))
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(nm)
  mkdirp.sync(tmp)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
