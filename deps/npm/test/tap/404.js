'use strict'
const path = require('path')
const test = require('tap').test
const Tacks = require('tacks')
const File = Tacks.File
const Dir = Tacks.Dir
const common = require('../common-tap.js')

const e404 = /test-npm-404@latest' is not in the npm registry/
const invalidPackage = /Your package name is not valid, because[\s\S]+1\. name can only contain URL-friendly characters/

const basedir = path.join(__dirname, path.basename(__filename, '.js'))
const testdir = path.join(basedir, 'testdir')
const cachedir = path.join(basedir, 'cache')
const globaldir = path.join(basedir, 'global')
const tmpdir = path.join(basedir, 'tmp')

const env = common.newEnv().extend({
  npm_config_cache: cachedir,
  npm_config_tmp: tmpdir,
  npm_config_prefix: globaldir,
  npm_config_registry: common.registry,
  npm_config_loglevel: 'error'
})

const fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'package.json': File({
      name: 'test',
      version: '1.0.0'
    })
  })
}))

function setup () {
  cleanup()
  fixture.create(basedir)
}

function cleanup () {
  fixture.remove(basedir)
}

test('setup', function (t) {
  setup()
  return common.fakeRegistry.listen()
})

test('404 message for basic package', function (t) {
  return common.npm(['install', 'test-npm-404'], {cwd: testdir, env}).then(([code, stdout, stderr]) => {
    t.is(code, 1, 'error code')
    t.match(stderr, e404, 'error output')
    t.notMatch(stderr, invalidPackage, 'no invalidity error')
  })
})

test('404 message for scoped package', function (t) {
  return common.npm(['install', '@npm/test-npm-404'], {cwd: testdir, env}).then(([code, stdout, stderr]) => {
    t.is(code, 1, 'error code')
    t.match(stderr, e404, 'error output')
    t.notMatch(stderr, invalidPackage, 'no invalidity error')
  })
})

test('cleanup', function (t) {
  common.fakeRegistry.close()
  cleanup()
  t.done()
})
