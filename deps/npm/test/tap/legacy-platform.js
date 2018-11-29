'use strict'
var test = require('tap').test
var common = require('../common-tap.js')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var basepath = common.pkg
var fixturepath = path.resolve(basepath, 'npm-test-platform')
var modulepath = path.resolve(basepath, 'node_modules')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

var fixture = new Tacks(
  Dir({
    README: File(
      'just an npm test\n'
    ),
    'package.json': File({
      name: 'npm-test-platform',
      version: '9.9.9-9',
      homepage: 'http://www.youtube.com/watch?v=dQw4w9WgXcQ',
      os: [
        '!this_is_not_a_real_os',
        '!neither_is_this'
      ],
      cpu: [
        '!this_is_not_a_real_cpu',
        '!this_isnt_either'
      ]
    })
  })
)

test('setup', function (t) {
  setup()
  t.done()
})

test('platform', function (t) {
  common.npm(['install', fixturepath], {cwd: basepath}, installCheckAndTest)
  function installCheckAndTest (err, code, stdout, stderr) {
    if (err) throw err
    console.error(stderr)
    t.is(code, 0, 'install went ok')
    t.done()
  }
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})

function setup () {
  cleanup()
  fixture.create(fixturepath)
  mkdirp.sync(modulepath)
}

function cleanup () {
  fixture.remove(fixturepath)
  rimraf.sync(basepath)
}
