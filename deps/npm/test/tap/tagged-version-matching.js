'use strict'
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var extend = Object.assign || require('util')._extend
var common = require('../common-tap.js')

var basedir = path.join(__dirname, path.basename(__filename, '.js'))
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

var conf = {
  cwd: testdir,
  env: extend({
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
    node_modules: Dir({
      example: Dir({
        'package.json': File({
          _from: 'example',
          _id: 'example@1.0.0',
          _requested: {
            raw: 'example@file:example',
            scope: null,
            escapedName: 'example',
            name: 'example',
            rawSpec: 'file:example',
            type: 'directory'
          },
          dependencies: {
            tagdep: 'latest',
            gitdep: 'npm/example-gitdep'
          },
          name: 'example',
          version: '1.0.0'
        })
      }),
      gitdep: Dir({
        'package.json': File({
          _from: 'npm/example-gitdep',
          _id: 'gitdep@1.0.0',
          _requested: {
            raw: 'gitdep@git://github.com/npm/example-gitdep.git#da39a3ee5e6b4b0d3255bfef95601890afd80709',
            scope: null,
            escapedName: 'gitdep',
            name: 'gitdep',
            rawSpec: 'git://github.com/npm/example-gitdep.git#da39a3ee5e6b4b0d3255bfef95601890afd80709',
            spec: 'git://github.com/npm/example-gitdep.git#da39a3ee5e6b4b0d3255bfef95601890afd80709',
            type: 'hosted',
            hosted: {
              type: 'github',
              ssh: 'git@github.com:npm/example-gitdep.git#da39a3ee5e6b4b0d3255bfef95601890afd80709',
              sshUrl: 'git+ssh://git@github.com/npm/example-gitdep.git#da39a3ee5e6b4b0d3255bfef95601890afd80709',
              httpsUrl: 'git+https://github.com/npm/example-gitdep.git#da39a3ee5e6b4b0d3255bfef95601890afd80709',
              gitUrl: 'git://github.com/npm/example-gitdep.git#da39a3ee5e6b4b0d3255bfef95601890afd80709',
              shortcut: 'github:npm/example-gitdep#da39a3ee5e6b4b0d3255bfef95601890afd80709',
              directUrl: 'https://raw.githubusercontent.com/npm/example-gitdep/da39a3ee5e6b4b0d3255bfef95601890afd80709/package.json'
            }
          },
          name: 'gitdep',
          version: '1.0.0'
        })
      }),
      tagdep: Dir({
        'package.json': File({
          _from: 'tagdep@latest',
          _id: 'tagdep@1.0.0',
          _requested: {
            raw: 'tagdep@https://registry.example.com/tagdep/-/tagdep-1.0.0.tgz',
            scope: null,
            escapedName: 'tagdep',
            name: 'tagdep',
            rawSpec: 'https://registry.example.com/tagdep/-/tagdep-1.0.0.tgz',
            spec: 'https://registry.example.com/tagdep/-/tagdep-1.0.0.tgz',
            type: 'remote'
          },
          name: 'tagdep',
          version: '1.0.0'
        })
      })
    }),
    'npm-shrinkwrap.json': File({
      name: 'tagged-version-matching',
      version: '1.0.0',
      dependencies: {
        tagdep: {
          version: '1.0.0',
          from: 'tagdep@latest',
          resolved: 'https://registry.example.com/tagdep/-/tagdep-1.0.0.tgz'
        },
        example: {
          version: '1.0.0',
          from: 'example'
        },
        gitdep: {
          version: '1.0.0',
          from: 'npm/example-gitdep',
          resolved: 'git://github.com/npm/example-gitdep.git#da39a3ee5e6b4b0d3255bfef95601890afd80709'
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
    var problems = result.problems || []
    // Original PR: https://github.com/npm/npm/pull/13941
    // Original issue: https://github.com/npm/npm/issues/13496
    // Original issue: https://github.com/npm/npm/issues/11736
    t.is(problems.length, 0, 'no problems')
    t.ok(!problems.some(function (err) { return /missing: tagdep/.test(err) }), 'tagged dependency matched ok')
    t.ok(!problems.some(function (err) { return /missing: gitdep/.test(err) }), 'git dependency matched ok')
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})
