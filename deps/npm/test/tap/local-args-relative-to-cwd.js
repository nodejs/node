'use strict'
var fs = require('graceful-fs')
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')
var testdir = path.join(__dirname, path.basename(__filename, '.js'))

var fixture = new Tacks(
  Dir({
    example: Dir({
    }),
    mod1: Dir({
      'package.json': File({
        name: 'mod1',
        version: '1.0.0'
      })
    }),
    node_modules: Dir({
    })
  })
)

function setup () {
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

function exists (file) {
  try {
    fs.statSync(file)
    return true
  } catch (ex) {
    return false
  }
}

test('local-args-relative-to-cwd', function (t) {
  var instdir = path.join(testdir, 'example')
  var config = ['--loglevel=error']
  common.npm(config.concat(['install', '../mod1']), {cwd: instdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.is(code, 0, 'install ran ok')
    t.ok(exists(path.join(testdir, 'node_modules', 'mod1')), 'mod1 installed')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
