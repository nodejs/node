'use strict'
var fs = require('fs')
var path = require('path')
var test = require('tap').test
var mr = require('npm-registry-mock')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')

var basedir = path.join(__dirname, path.basename(__filename, '.js'))
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

var pkgLockPath = path.join(testdir, 'package-lock.json')
var nodeModulesPath = path.join(testdir, 'node_modules')

var conf = {
  cwd: testdir,
  env: Object.assign({}, process.env, {
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
}

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
  setup()
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    t.done()
  })
})

test('package-lock-only', function (t) {
  return common.npm(['install', '--package-lock-only'], conf).spread((code, stdout, stderr) => {
    t.is(code, 0, 'command ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    // Verify that node_modules does not exist
    t.notOk(fs.existsSync(nodeModulesPath), 'ensure that node_modules does not exist')

    // Verify that package-lock.json exists and has `mkdirp@0.3.5` in it.
    t.ok(fs.existsSync(pkgLockPath), 'ensure that package-lock.json was created')
    var pkgLock = JSON.parse(fs.readFileSync(pkgLockPath, 'utf-8'))
    t.equal(pkgLock.dependencies.mkdirp.version, '0.3.5')
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
