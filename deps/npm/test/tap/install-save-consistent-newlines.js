'use strict'

const fs = require('graceful-fs')
const path = require('path')

const mkdirp = require('mkdirp')
const mr = require('npm-registry-mock')
const osenv = require('osenv')
const rimraf = require('rimraf')
const test = require('tap').test

const common = require('../common-tap.js')

const pkg = path.join(__dirname, 'install-save-consistent-newlines')

const EXEC_OPTS = { cwd: pkg }

const json = {
  name: 'install-save-consistent-newlines',
  version: '0.0.1',
  description: 'fixture'
}

var server

test('setup', function (t) {
  setup('\n')
  mr({ port: common.port }, function (er, s) {
    server = s
    t.end()
  })
})

test('\'npm install --save\' should keep the original package.json line endings (LF)', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      '--registry', common.registry,
      '--save',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm ran without issue')
      t.notOk(code, 'npm install exited without raising an error code')

      const pkgPath = path.resolve(pkg, 'package.json')
      const pkgStr = fs.readFileSync(pkgPath, 'utf8')

      t.match(pkgStr, '\n')
      t.notMatch(pkgStr, '\r')

      const pkgLockPath = path.resolve(pkg, 'package-lock.json')
      const pkgLockStr = fs.readFileSync(pkgLockPath, 'utf8')

      t.match(pkgLockStr, '\n')
      t.notMatch(pkgLockStr, '\r')

      t.end()
    }
  )
})

test('\'npm install --save\' should keep the original package.json line endings (CRLF)', function (t) {
  setup('\r\n')

  common.npm(
    [
      '--loglevel', 'silent',
      '--registry', common.registry,
      '--save',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm ran without issue')
      t.notOk(code, 'npm install exited without raising an error code')

      const pkgPath = path.resolve(pkg, 'package.json')
      const pkgStr = fs.readFileSync(pkgPath, 'utf8')

      t.match(pkgStr, '\r\n')
      t.notMatch(pkgStr, /[^\r]\n/)

      const pkgLockPath = path.resolve(pkg, 'package-lock.json')
      const pkgLockStr = fs.readFileSync(pkgLockPath, 'utf8')

      t.match(pkgLockStr, '\r\n')
      t.notMatch(pkgLockStr, /[^\r]\n/)

      t.end()
    }
  )
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

function setup (lineEnding) {
  cleanup()
  mkdirp.sync(path.resolve(pkg, 'node_modules'))

  var jsonStr = JSON.stringify(json, null, 2)

  if (lineEnding === '\r\n') {
    jsonStr = jsonStr.replace(/\n/g, '\r\n')
  }

  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    jsonStr
  )
  process.chdir(pkg)
}
