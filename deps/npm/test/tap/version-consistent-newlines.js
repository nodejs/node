'use strict'

const common = require('../common-tap.js')
const test = require('tap').test
const npm = require('../../')
const osenv = require('osenv')
const path = require('path')
const fs = require('fs')
const mkdirp = require('mkdirp')
const rimraf = require('rimraf')
const requireInject = require('require-inject')

const pkg = common.pkg
const cache = path.resolve(pkg, 'cache')
const gitDir = path.resolve(pkg, '.git')

test('npm version does not alter the line endings in package.json (LF)', function (t) {
  setup('\n')

  npm.load({cache: cache, registry: common.registry}, function () {
    const version = requireInject('../../lib/version', {
      which: function (cmd, cb) {
        process.nextTick(function () {
          cb(new Error('ENOGIT!'))
        })
      }
    })

    version(['patch'], function (err) {
      if (!t.error(err)) return t.end()

      const pkgPath = path.resolve(pkg, 'package.json')
      const pkgStr = fs.readFileSync(pkgPath, 'utf8')

      t.match(pkgStr, '\n')
      t.notMatch(pkgStr, '\r')

      t.end()
    })
  })
})

test('npm version does not alter the line endings in package.json (CRLF)', function (t) {
  setup('\r\n')

  npm.load({cache: cache, registry: common.registry}, function () {
    const version = requireInject('../../lib/version', {
      which: function (cmd, cb) {
        process.nextTick(function () {
          cb(new Error('ENOGIT!'))
        })
      }
    })

    version(['patch'], function (err) {
      if (!t.error(err)) return t.end()

      const pkgPath = path.resolve(pkg, 'package.json')
      const pkgStr = fs.readFileSync(pkgPath, 'utf8')

      t.match(pkgStr, '\r\n')
      t.notMatch(pkgStr, /[^\r]\n/)

      t.end()
    })
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())

  rimraf.sync(pkg)
  t.end()
})

function setup (lineEnding) {
  mkdirp.sync(pkg)
  mkdirp.sync(cache)
  mkdirp.sync(gitDir)
  fs.writeFileSync(
    path.resolve(pkg, 'package.json'),
    JSON.stringify({
      author: 'Terin Stock',
      name: 'version-no-git-test',
      version: '0.0.0',
      description: "Test for npm version if git binary doesn't exist"
    }, null, 2).replace(/\n/g, lineEnding),
    'utf8'
  )
  process.chdir(pkg)
}
