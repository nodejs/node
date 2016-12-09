'use strict'
var fs = require('fs')
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')
var npm = require('../../lib/npm.js')
var tar = require('../../lib/utils/tar.js')

var testdir = path.join(__dirname, path.basename(__filename, '.js'))
var packed = path.join(testdir, 'packed')

var fixture = new Tacks(
  Dir({
    'package.json': File({
      name: 'bundled-transitive-deps',
      version: '1.0.0',
      dependencies: {
        'a': '1.0.0'
      },
      bundleDependencies: [
        'a'
      ]
    }),
    node_modules: Dir({
      'a': Dir({
        'package.json': File({
          name: 'a',
          version: '1.0.0',
          dependencies: {
            'b': '1.0.0'
          }
        })
      }),
      'b': Dir({
        'package.json': File({
          name: 'b',
          version: '1.0.0'
        })
      })
    })
  })
)

function setup () {
  cleanup()
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

test('setup', function (t) {
  setup()
  npm.load({}, t.end)
})

test('bundled-transitive-deps', function (t) {
  common.npm(['pack'], {cwd: testdir}, thenCheckPack)
  function thenCheckPack (err, code, stdout, stderr) {
    if (err) throw err
    var tarball = stdout.trim()
    t.comment(stderr.trim())
    t.is(code, 0, 'pack successful')
    tar.unpack(path.join(testdir, tarball), packed, thenCheckContents)
  }
  function thenCheckContents (err) {
    t.ifError(err, 'unpack successful')
    var transitivePackedDep = path.join(packed, 'node_modules', 'b')
    t.doesNotThrow(transitivePackedDep + ' exists', function () {
      fs.statSync(transitivePackedDep)
    })
    t.end()
  }
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
