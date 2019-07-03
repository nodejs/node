'use strict'
var test = require('tap').test
var common = require('../common-tap.js')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var basepath = common.pkg
var fixturepath = path.resolve(basepath, 'npm-test-test-package')
var modulepath = path.resolve(basepath, 'node_modules')
var installedpath = path.resolve(modulepath, 'npm-test-test-package')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

var fixture = new Tacks(
  Dir({
    README: File(
      'just an npm test\n'
    ),
    'package.json': File({
      name: 'npm-test-test-package',
      author: 'Testy McMock',
      version: '1.2.3-99-b',
      description: "This is a test package used for debugging. It has some random data and that's all."
    })
  })
)

test('setup', function (t) {
  setup()
  t.done()
})

test('test-package', function (t) {
  common.npm(['install', fixturepath], {cwd: basepath}, installCheckAndTest)

  function installCheckAndTest (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'install went ok')
    common.npm(['test'], {cwd: installedpath}, testCheckAndRemove)
  }

  function testCheckAndRemove (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'npm test w/o test is ok')
    common.npm(['rm', fixturepath], {cwd: basepath}, removeCheckAndDone)
  }

  function removeCheckAndDone (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'remove went ok')
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
