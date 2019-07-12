'use strict'
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var Symlink = Tacks.Symlink
var common = require('../common-tap.js')

var basedir = common.pkg
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

var conf = {
  cwd: testdir,
  env: Object.assign({
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  }, process.env)
}

var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    example: Dir({
      'package.json': File({
        dependencies: {
          tagdep: 'latest',
          gitdep: 'npm/example-gitdep'
        },
        name: 'example',
        version: '1.0.0'
      })
    }),
    node_modules: Dir({
      example: Symlink('../example'),
      gitdep: Dir({
        'package.json': File({
          _from: 'npm/example-gitdep',
          _id: 'gitdep@1.0.0',
          _resolved: 'github:npm/example-gitdep#da39a3ee5e6b4b0d3255bfef95601890afd80709',
          name: 'gitdep',
          version: '1.0.0'
        })
      }),
      tagdep: Dir({
        'package.json': File({
          _from: 'tagdep@latest',
          _id: 'tagdep@1.0.0',
          _integrity: 'sha1-0EJSKmsdk39848LlrRg/hZQo2B8=',
          _resolved: 'https://registry.example.com/tagdep/-/tagdep-1.0.0.tgz',
          name: 'tagdep',
          version: '1.0.0'
        })
      })
    }),
    'npm-shrinkwrap.json': File({
      name: 'tagged-version-matching',
      version: '1.0.0',
      lockfileVersion: 1,
      requires: true,
      dependencies: {
        tagdep: {
          version: '1.0.0',
          resolved: 'https://registry.example.com/tagdep/-/tagdep-1.0.0.tgz',
          integrity: 'sha1-0EJSKmsdk39848LlrRg/hZQo2B8='
        },
        example: {
          version: 'file:example',
          requires: {
            tagdep: '^1.0.0',
            gitdep: 'github:npm/example-gitdep#da39a3ee5e6b4b0d3255bfef95601890afd80709'
          }
        },
        gitdep: {
          version: 'github:npm/example-gitdep#da39a3ee5e6b4b0d3255bfef95601890afd80709'
        }
      }
    }),
    'package.json': File({
      name: 'tagged-version-matching',
      version: '1.0.0',
      dependencies: {
        example: 'file:example',
        gitdep: 'npm/example-gitdep'
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
  t.done()
})

test('tagged-version-matching', function (t) {
  common.npm(['ls', '--json'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    if (stderr.trim()) t.comment(stderr.trim())
    var result = JSON.parse(stdout.trim())
    var expected = {
      name: 'tagged-version-matching',
      version: '1.0.0',
      dependencies: {
        example: {
          version: '1.0.0',
          dependencies: {
            gitdep: {
              version: '1.0.0',
              from: 'npm/example-gitdep'
            },
            tagdep: {
              version: '1.0.0',
              from: 'tagdep@latest'
            }
          }
        },
        gitdep: {
          version: '1.0.0',
          from: 'npm/example-gitdep'
        }
      }
    }

    t.like(result, expected, 'ls looks ok')
    t.is((result.problems || []).length, 0, 'no problems')
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})
