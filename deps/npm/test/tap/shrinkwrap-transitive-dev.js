'use strict'
var fs = require('fs')
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var common = require('../common-tap.js')
var File = Tacks.File
var Dir = Tacks.Dir

var testname = path.basename(__filename, '.js')
var testdir = path.join(__dirname, testname)
var cachedir = path.join(testdir, 'cache')
var swfile = path.join(testdir, 'npm-shrinkwrap.json')
var fixture = new Tacks(
  Dir({
    cache: Dir(),
    mods: Dir({
      moda: Dir({
        'package.json': File({
          name: 'moda',
          version: '1.0.0',
          dependencies: {
            modb: '../modb'
          }
        })
      }),
      modb: Dir({
        'package.json': File({
          name: 'modb',
          version: '1.0.0'
        })
      })
    }),
    'package.json': File({
      name: testname,
      version: '1.0.0',
      devDependencies: {
        moda: 'file:mods/moda'
      }
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
  common.npm(['install', '--cache=' + cachedir], {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'setup ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.end()
  })
})

test('transitive-deps-of-dev-deps', function (t) {
  common.npm(['shrinkwrap', '--loglevel=error', '--only=prod'], {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'shrinkwrap ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    try {
      var shrinkwrap = JSON.parse(fs.readFileSync(swfile))
      t.isDeeply(shrinkwrap.dependencies, {}, 'empty shrinkwrap')
    } catch (ex) {
      t.ifError(ex)
    }
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
