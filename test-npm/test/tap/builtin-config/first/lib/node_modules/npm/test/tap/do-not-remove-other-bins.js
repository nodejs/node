'use strict'
var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')

var base = path.resolve(__dirname, path.basename(__filename, '.js'))
var installPath = path.resolve(base, 'install')
var installBin = path.resolve(installPath, 'node_modules', '.bin')
var packageApath = path.resolve(base, 'packageA')
var packageBpath = path.resolve(base, 'packageB')

var packageA = {
  name: 'a',
  version: '1.0.0',
  description: 'x',
  repository: 'x',
  license: 'MIT',
  bin: {
    testbin: './testbin.js'
  }
}
var packageB = {
  name: 'b',
  version: '1.0.0',
  description: 'x',
  repository: 'x',
  license: 'MIT',
  bin: {
    testbin: './testbin.js'
  }
}

var EXEC_OPTS = {
  cwd: installPath
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(path.join(installPath, 'node_modules'))
  mkdirp.sync(packageApath)
  fs.writeFileSync(
    path.join(packageApath, 'package.json'),
    JSON.stringify(packageA, null, 2)
  )
  fs.writeFileSync(
    path.join(packageApath, packageA.bin.testbin),
    ''
  )
  mkdirp.sync(packageBpath)
  fs.writeFileSync(
    path.join(packageBpath, 'package.json'),
    JSON.stringify(packageB, null, 2)
  )
  fs.writeFileSync(
    path.join(packageBpath, packageB.bin.testbin),
    ''
  )
  t.end()
})

test('npm install A', function (t) {
  process.chdir(installPath)
  common.npm([
    'install', packageApath
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    console.log(stdout, stderr)
    t.ifErr(err, 'install finished successfully')
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    t.end()
  })
})

test('npm install B', function (t) {
  process.chdir(installPath)
  common.npm([
    'install', packageBpath
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifErr(err, 'install finished successfully')
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    t.end()
  })
})

test('verify bins', function (t) {
  var bin = path.dirname(
    path.resolve(
      installBin,
      common.readBinLink(path.join(installBin, 'testbin'))))
  t.is(bin, path.join(installPath, 'node_modules', 'b'))
  t.end()
})

test('rm install A', function (t) {
  process.chdir(installPath)
  common.npm([
    'rm', packageApath
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifErr(err, 'install finished successfully')
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    t.end()
  })
})

test('verify postremoval bins', function (t) {
  var bin = path.dirname(
    path.resolve(
      installBin,
      common.readBinLink(path.join(installBin, 'testbin'))))
  t.is(bin, path.join(installPath, 'node_modules', 'b'))
  t.end()
})

test('cleanup', function (t) {
  cleanup()
  t.pass('cleaned up')
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(base)
}
