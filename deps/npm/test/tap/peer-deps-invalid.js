var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test
var Tacks = require('tacks')
var Dir = Tacks.Dir
var File = Tacks.File

var npm = require('../../')
var common = require('../common-tap')

var testdir = path.resolve(__dirname, path.basename(__filename, '.js'))
var cachedir = path.resolve(testdir, 'cache')

var fixtures = new Tacks(Dir({
  cache: Dir({}),
  'package.json': File({
    author: 'Domenic Denicola <domenic@domenicdenicola.com> (http://domenicdenicola.com/)',
    name: 'peer-deps-invalid',
    version: '0.0.0',
    dependencies: {
      'npm-test-peer-deps-file': 'file-ok/',
      'npm-test-peer-deps-file-invalid': 'file-fail/'
    }
  }),
  'file-ok': Dir({
    'package.json': File({
      name: 'npm-test-peer-deps-file',
      main: 'index.js',
      version: '1.2.3',
      description:'This one should conflict with the other one',
      peerDependencies: { underscore: '1.3.1' },
      dependencies: { mkdirp: '0.3.5' }
    }),
    'index.js': File(
      "module.exports = 'I\'m just a lonely index, naked as the day I was born.'"
    ),
  }),
  'file-fail': Dir({
    'package.json': File({
      name: 'npm-test-peer-deps-file-invalid',
      main: 'index.js',
      version: '1.2.3',
      description:'This one should conflict with the other one',
      peerDependencies: { underscore: '1.3.3' }
    }),
    'index.js': File(
      "module.exports = 'I\'m just a lonely index, naked as the day I was born.'"
    ),
  }),
}))

test('setup', function (t) {
  cleanup()
  fixtures.create(testdir)
  process.chdir(testdir)
  t.end()
})

test('installing dependencies that have conflicting peerDependencies', function (t) {
  mr({port: common.port}, function (err, s) { // create mock registry.
    t.ifError(err, 'mock registry started')
    npm.load({
      cache: cachedir,
      registry: common.registry
    }, function () {
      npm.commands.install([], function (err) {
        if (!err) {
          t.fail("No error!")
        } else {
          t.equal(err.code, "EPEERINVALID")
          t.equal(err.packageName, "underscore")
          t.match(err.packageVersion, /^1\.3\.[13]$/)
          t.match(err.message, /^The package underscore@1\.3\.[13] does not satisfy its siblings' peerDependencies requirements!$/)
        }
        s.close() // shutdown mock registry.
        t.end()
      })
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  fixtures.remove(testdir)
}
