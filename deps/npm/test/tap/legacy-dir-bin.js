'use strict'
var test = require('tap').test
var common = require('../common-tap.js')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var basepath = path.resolve(__dirname, path.basename(__filename, '.js'))
var fixturepath = path.resolve(basepath, 'npm-test-dir-bin')
var modulepath = path.resolve(basepath, 'node_modules')
var installedpath = path.resolve(modulepath, 'npm-test-dir-bin')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var fixture = new Tacks(
  Dir({
    bin: Dir({
      'dir-bin': File(
        '#!/usr/bin/env node\n' +
        "console.log('test ran ok')\n"
      )
    }),
    'package.json': File({
      name: 'npm-test-dir-bin',
      version: '1.2.5',
      directories: {
        bin: './bin'
      },
      scripts: {
        test: 'node test.js'
      }
    }),
    'test.js': File(
      "require('child_process').exec('dir-bin', { env: process.env },\n" +
      '  function (err, stdout, stderr) {\n' +
      "    if (err && err.code) throw new Error('exited badly with code = ' + err.code)\n" +
      '    console.log(stdout)\n' +
      '    console.error(stderr)\n' +
      '  }\n' +
      ')\n'
    )
  })
)
test('setup', function (t) {
  setup()
  t.done()
})
test('dir-bin', function (t) {
  common.npm(['install', fixturepath], {cwd: basepath}, installCheckAndTest)
  function installCheckAndTest (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'install went ok')
    common.npm(['test'], {cwd: installedpath}, testCheckAndRemove)
  }
  function testCheckAndRemove (err, code, stdout, stderr) {
    t.ifError(err, 'npm test on array bin')
    t.equal(code, 0, 'exited OK')
    t.equal(stderr.trim(), '', 'no error output')
    t.match(stdout, /test ran ok/, 'child script ran properly')
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
