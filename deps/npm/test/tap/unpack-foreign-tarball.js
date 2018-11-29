var fs = require('graceful-fs')
var path = require('path')

var t = require('tap')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')

var common = require('../common-tap.js')

var fixtures = path.resolve(__dirname, '..', 'fixtures')

var pkg = common.pkg
var nm = path.resolve(pkg, 'node_modules')
var target = path.resolve(nm, 'npm-test-gitignore')
var cache = common.cache
var tmp = path.resolve(pkg, 'tmp')

var EXEC_OPTS = {
  env: {
    'npm_config_cache': cache,
    'npm_config_tmp': tmp
  },
  cwd: pkg
}

function verify (t, files, code) {
  if (code) {
    return t.fail('exited with failure: ' + code)
  }
  var actual = fs.readdirSync(target).sort()
  var expect = files.concat(['.npmignore', 'package.json']).sort()
  t.same(actual, expect)
}

t.comment('test for https://github.com/npm/npm/issues/5658')

t.test('npmignore only', function (t) {
  t.test('setup', setup)
  var file = path.resolve(fixtures, 'npmignore.tgz')
  return t.test('test', t => common.npm(['install', file], EXEC_OPTS)
    .then(([code]) => verify(t, ['foo'], code)))
})

t.test('gitignore only', function (t) {
  t.test('setup', setup)
  var file = path.resolve(fixtures, 'gitignore.tgz')
  return t.test('test', t => common.npm(['install', file], EXEC_OPTS)
    .then(([code]) => verify(t, ['foo'], code)))
})

t.test('gitignore and npmignore', function (t) {
  t.test('setup', setup)
  var file = path.resolve(fixtures, 'gitignore-and-npmignore.tgz')
  return t.test('test', t => common.npm(['install', file], EXEC_OPTS)
    .then(([code]) => verify(t, ['foo', 'bar'], code)))
})

t.test('gitignore and npmignore, not gzipped 1/2', function (t) {
  t.test('setup', setup)
  var file = path.resolve(fixtures, 'gitignore-and-npmignore.tar')
  return t.test('test', t => common.npm(['install', file], EXEC_OPTS)
    .then(([code]) => verify(t, ['foo', 'bar'], code)))
})

t.test('gitignore and npmignore, not gzipped 2/2', function (t) {
  t.test('setup', setup)
  var file = path.resolve(fixtures, 'gitignore-and-npmignore-2.tar')
  return t.test('test', t => common.npm(['install', file], EXEC_OPTS)
    .then(([code]) => verify(t, ['foo', 'bar'], code)))
})

function setup (t) {
  t.test('destroy', t => {
    t.plan(2)
    t.test('node_modules', t => rimraf(nm, t.end))
    t.test('tmp', t => rimraf(tmp, t.end))
  })
  t.test('create', t => {
    mkdirp.sync(nm)
    mkdirp.sync(tmp)
    t.end()
  })
  t.end()
}
