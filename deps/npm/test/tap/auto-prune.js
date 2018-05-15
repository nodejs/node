'use strict'
const path = require('path')
const test = require('tap').test
const mr = require('npm-registry-mock')
const Tacks = require('tacks')
const File = Tacks.File
const Dir = Tacks.Dir
const extend = Object.assign || require('util')._extend
const common = require('../common-tap.js')

const basedir = path.join(__dirname, path.basename(__filename, '.js'))
const testdir = path.join(basedir, 'testdir')
const cachedir = path.join(basedir, 'cache')
const globaldir = path.join(basedir, 'global')
const tmpdir = path.join(basedir, 'tmp')

const conf = {
  cwd: testdir,
  env: extend(extend({}, process.env), {
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
}

let server
const fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    node_modules: Dir({
      minimist: Dir({
        'package.json': File({
          _integrity: 'sha1-hX/Kv8M5fSYluCKCYuhqp6ARsF0=',
          _resolved: 'https://registry.npmjs.org/minimist/-/minimist-0.0.8.tgz',
          name: 'minimist',
          version: '0.0.8'
        })
      }),
      mkdirp: Dir({
        'package.json': File({
          _integrity: 'sha1-MAV0OOrGz3+MR2fzhkjWaX11yQM=',
          _resolved: 'https://registry.npmjs.org/mkdirp/-/mkdirp-0.5.1.tgz',
          dependencies: {
            minimist: '0.0.8'
          },
          name: 'mkdirp',
          version: '0.5.1'
        })
      }),
      null: Dir({
        'package.json': File({
          _integrity: 'sha1-WoIdUnAxMlyG06AasQFzKgkfoew=',
          _resolved: 'https://registry.npmjs.org/null/-/null-1.0.1.tgz',
          _spec: 'null',
          name: 'null',
          version: '1.0.1'
        })
      })
    }),
    'package-lock.json': File({
      name: 'with-lock',
      version: '1.0.0',
      lockfileVersion: 1,
      requires: true,
      dependencies: {
        minimist: {
          version: '0.0.8',
          resolved: 'https://registry.npmjs.org/minimist/-/minimist-0.0.8.tgz',
          integrity: 'sha1-hX/Kv8M5fSYluCKCYuhqp6ARsF0='
        },
        mkdirp: {
          version: '0.5.1',
          resolved: 'https://registry.npmjs.org/mkdirp/-/mkdirp-0.5.1.tgz',
          integrity: 'sha1-MAV0OOrGz3+MR2fzhkjWaX11yQM=',
          requires: {
            minimist: '0.0.8'
          }
        }
      }
    }),
    'package.json': File({
      name: 'with-lock',
      version: '1.0.0',
      dependencies: {
        mkdirp: '^0.5.1'
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

test('auto-prune w/ package-lock', function (t) {
  common.npm(['install', '--dry-run', '--json'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stderr.trim())
    const result = JSON.parse(stdout)
    t.is(result.added.length, 0, 'nothing added')
    t.is(result.updated.length, 0, 'nothing updated')
    t.is(result.moved.length, 0, 'nothing moved')
    t.is(result.failed.length, 0, 'nothing failed')
    t.is(result.removed.length, 1, 'pruned 1')
    t.like(result, {'removed': [{'name': 'null'}]}, 'pruned the right one')
    t.done()
  })
})

test('auto-prune w/ --no-package-lock', function (t) {
  common.npm(['install', '--dry-run', '--json', '--no-package-lock'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stderr.trim())
    const result = JSON.parse(stdout)
    t.is(result.added.length, 0, 'nothing added')
    t.is(result.updated.length, 0, 'nothing updated')
    t.is(result.moved.length, 0, 'nothing moved')
    t.is(result.failed.length, 0, 'nothing failed')
    t.is(result.removed.length, 0, 'nothing pruned')
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
