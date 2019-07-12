'use strict'
const path = require('path')
const test = require('tap').test
const mr = require('npm-registry-mock')
const Tacks = require('tacks')
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
  env: Object.assign({}, process.env, {
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
}

function exampleManifest (version) {
  return {
    name: 'example',
    version: version
  }
}

const examplePackument = {
  'name': 'example',
  'dist-tags': {
    'latest': '1.2.4',
    'beta': '1.2.6'
  },
  'versions': {
    '1.2.0': exampleManifest('1.2.0'),
    '1.2.1': exampleManifest('1.2.1'),
    '1.2.2': exampleManifest('1.2.2'),
    '1.2.3': exampleManifest('1.2.3'),
    '1.2.4': exampleManifest('1.2.4'),
    '1.2.5': exampleManifest('1.2.5'),
    '1.2.6': exampleManifest('1.2.6')
  }
}

const fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    node_modules: Dir({
      example: Dir({
        'package.json': File({
          name: 'example',
          version: '1.2.3'
        })
      })
    }),
    'package.json': File({
      name: 'outdated-latest',
      version: '1.0.0',
      dependencies: {
        example: '^1.2.0'
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

let server

test('setup', function (t) {
  setup()
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    server.get('/example').reply(200, examplePackument)
    t.done()
  })
})

test('example', function (t) {
  return common.npm(['outdated', '--json'], conf).spread((code, stdout, stderr) => {
    t.is(code, 1, 'files ARE outdated!')
    const result = JSON.parse(stdout.trim())
    t.comment(stderr.trim())
    // your assertions here
    t.like(result, {example: {current: '1.2.3', wanted: '1.2.4', latest: '1.2.4'}}, 'got latest, not beta')
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
