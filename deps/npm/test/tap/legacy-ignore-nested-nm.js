'use strict'
var test = require('tap').test
var common = require('../common-tap.js')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var basepath = path.resolve(__dirname, path.basename(__filename, '.js'))
var fixturepath = path.resolve(basepath, 'npm-test-ignore-nested-nm')
var modulepath = path.resolve(basepath, 'node_modules')
var installedpath = path.resolve(modulepath, 'npm-test-ignore-nested-nm')
var fs = require('graceful-fs')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

var fileData = 'I WILL NOT BE IGNORED!\n'
var fixture = new Tacks(
  Dir({
    lib: Dir({
      node_modules: Dir({
        foo: File(fileData)
      })
    }),
    'package.json': File({
      name: 'npm-test-ignore-nested-nm',
      version: '1.2.5'
    })
  })
)
test('setup', function (t) {
  setup()
  t.done()
})
test('ignore-nested-nm', function (t) {
  common.npm(['install', fixturepath], {cwd: basepath}, installCheckAndTest)
  function installCheckAndTest (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'install went ok')
    var foopath = path.resolve(installedpath, 'lib/node_modules/foo')
    fs.readFile(foopath, function (err, data) {
      t.ifError(err, 'file read successfully')
      t.equal(data.toString(), fileData)
      common.npm(['rm', fixturepath], {cwd: basepath}, removeCheckAndDone)
    })
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
