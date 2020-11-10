'use strict'
const path = require('path')
const test = require('tap').test
const Tacks = require('tacks')
const File = Tacks.File
const Dir = Tacks.Dir
const common = require('../common-tap.js')

const basedir = common.pkg
const testdir = path.join(basedir, 'testdir')
const cachedir = common.cache
const globaldir = path.join(basedir, 'global')
const tmpdir = path.join(basedir, 'tmp')

const conf = {
  cwd: testdir,
  env: common.newEnv().extend({
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
}

const fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    node_modules: Dir({
      'foo-http': Dir({
        'package.json': File({
          name: 'foo-http',
          version: '1.0.0'
        })
      }),
      'foo-https': Dir({
        'package.json': File({
          name: 'foo-https',
          version: '1.0.0'
        })
      })
    }),
    'package.json': File({
      name: 'outdated-remote',
      version: '1.0.0',
      dependencies: {
        'foo-http': 'http://example.com/foo.tar.gz',
        'foo-https': 'https://example.com/foo.tar.gz'
      }
    })
  })
}))

function setup () {
  fixture.create(basedir)
}

test('setup', t => {
  setup()
  return common.fakeRegistry.listen()
})

test('discovers new versions in outdated', t => {
  return common.npm(['outdated', '--json'], conf).then(([code, stdout, stderr]) => {
    t.is(code, 1, 'npm outdated completed successfully')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    const data = JSON.parse(stdout)
    t.same(data['foo-http'], {
      current: '1.0.0',
      wanted: 'remote',
      latest: 'remote',
      location: ''
    })
    t.same(data['foo-https'], {
      current: '1.0.0',
      wanted: 'remote',
      latest: 'remote',
      location: ''
    })
  })
})

test('cleanup', t => {
  common.fakeRegistry.close()
  t.done()
})
