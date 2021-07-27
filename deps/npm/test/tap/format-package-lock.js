'use strict'
const fs = require('fs')
const path = require('path')
const test = require('tap').test
const mr = require('npm-registry-mock')
const Tacks = require('tacks')
const File = Tacks.File
const Dir = Tacks.Dir
const common = require('../common-tap.js')

const basedir = common.pkg
const testdir = path.join(basedir, 'testdir')
const cachedir = common.cache
const globaldir = path.join(basedir, 'global')
const tmpdir = path.join(basedir, 'tmp')

const pkgPath = path.join(testdir, 'package.json')
const pkgLockPath = path.join(testdir, 'package-lock.json')
const shrinkwrapPath = path.join(testdir, 'npm-shrinkwrap.json')
const CRLFreg = /\r\n|\r|\n/

const env = common.newEnv().extend({
  npm_config_cache: cachedir,
  npm_config_tmp: tmpdir,
  npm_config_prefix: globaldir,
  npm_config_registry: common.registry,
  npm_config_loglevel: 'warn',
  npm_config_format_package_lock: false
})

var server
var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'package.json': File({
      name: 'install-package-lock-only',
      version: '1.0.0',
      dependencies: {
        mkdirp: '^0.3.4'
      }
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
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    t.done()
  })
})

test('package-lock.json unformatted, package.json formatted when config has `format-package-lock: false`', function (t) {
  setup()
  common.npm(['install'], {cwd: testdir, env}).spread((code, stdout, stderr) => {
    t.is(code, 0, 'ok')
    t.ok(fs.existsSync(pkgLockPath), 'ensure that package-lock.json was created')
    const pkgLockUtf8 = fs.readFileSync(pkgLockPath, 'utf-8')
    t.equal(pkgLockUtf8.split(CRLFreg).length, 2, 'package-lock.json is unformatted')
    const pkgUtf8 = fs.readFileSync(pkgPath, 'utf-8')
    t.notEqual(pkgUtf8.split(CRLFreg).length, 2, 'package.json is formatted')
    t.done()
  })
})

test('npm-shrinkwrap.json unformatted when config has `format-package-lock: false`', function (t) {
  setup()
  common.npm(['shrinkwrap'], {cwd: testdir, env}).spread((code, stdout, stderr) => {
    t.is(code, 0, 'ok')
    t.ok(fs.existsSync(shrinkwrapPath), 'ensure that npm-shrinkwrap.json was created')
    const shrinkwrapUtf8 = fs.readFileSync(shrinkwrapPath, 'utf-8')
    t.equal(shrinkwrapUtf8.split(CRLFreg).length, 2, 'npm-shrinkwrap.json is unformatted')
    t.done()
  })
})

test('package-lock.json and package.json formatted when config has `format-package-lock: true`', function (t) {
  setup()
  common.npm(['install'], {cwd: testdir}).spread((code, stdout, stderr) => {
    t.is(code, 0, 'ok')
    t.ok(fs.existsSync(pkgLockPath), 'ensure that package-lock.json was created')
    const pkgLockUtf8 = fs.readFileSync(pkgLockPath, 'utf-8')
    t.notEqual(pkgLockUtf8.split(CRLFreg).length, 2, 'package-lock.json is formatted')
    const pkgUtf8 = fs.readFileSync(pkgPath, 'utf-8')
    t.notEqual(pkgUtf8.split(CRLFreg).length, 2, 'package.json is formatted')
    t.done()
  })
})

test('npm-shrinkwrap.json formatted when config has `format-package-lock: true`', function (t) {
  setup()
  common.npm(['shrinkwrap'], {cwd: testdir}).spread((code, stdout, stderr) => {
    t.is(code, 0, 'ok')
    t.ok(fs.existsSync(shrinkwrapPath), 'ensure that npm-shrinkwrap.json was created')
    const shrinkwrapUtf8 = fs.readFileSync(shrinkwrapPath, 'utf-8')
    t.notEqual(shrinkwrapUtf8.split(CRLFreg).length, 2, 'npm-shrinkwrap.json is unformatted')
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
