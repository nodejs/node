'use strict'
const path = require('path')
const test = require('tap').test
const mr = require('npm-registry-mock')
const Tacks = require('tacks')
const fs = require('fs')
const File = Tacks.File
const Dir = Tacks.Dir
const common = require('../common-tap.js')

const basedir = common.pkg
const testdir = path.join(basedir, 'testdir')
const cachedir = path.join(basedir, 'cache')
const globaldir = path.join(basedir, 'global')
const tmpdir = path.join(basedir, 'tmp')

const conf = {
  cwd: testdir,
  stdio: [0, 1, 2],
  env: Object.assign({}, process.env, {
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'silly'
  })
}

let server
const fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    example: Dir({
      'package.json': File({
        name: 'example',
        version: '1.0.0'
      })
    }),
    'package.json': File({
      name: 'save-optional',
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
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    t.done()
  })
})

test('example', function (t) {
  common.npm(['install', '-O', '--package-lock-only', 'file:example'], conf, function (err, code) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    const plock = JSON.parse(fs.readFileSync(`${testdir}/package-lock.json`))
    t.like(plock, { dependencies: { example: { optional: true } } }, 'optional status saved')
    // your assertions here
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
