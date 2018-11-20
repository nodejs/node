'use strict'
var path = require('path')
var fs = require('fs')
var test = require('tap').test
var common = require('../common-tap.js')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var basepath = path.resolve(__dirname, path.basename(__filename, '.js'))
var fixturepath = path.resolve(basepath, 'npm-test-missing-bindir')
var modulepath = path.resolve(basepath, 'node_modules')
var installedpath = path.resolve(modulepath, 'npm-test-missing-bindir')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

var fixture = new Tacks(
  Dir({
    README: File(
      'just an npm test\n'
    ),
    'package.json': File({
      name: 'npm-test-missing-bindir',
      version: '0.0.0',
      directories: {
        bin: './not-found'
      }
    })
  })
)

test('setup', function (t) {
  setup()
  t.done()
})

function installedExists (filename) {
  try {
    fs.statSync(path.resolve(installedpath, filename))
    return true
  } catch (ex) {
    console.log(ex)
    return false
  }
}

test('missing-bindir', function (t) {
  common.npm(['install', fixturepath], {cwd: basepath}, installCheckAndTest)

  function installCheckAndTest (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'install went ok')
    t.is(installedExists('README'), true, 'README')
    t.is(installedExists('package.json'), true, 'package.json')
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
