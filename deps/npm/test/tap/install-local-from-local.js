'use strict'
var path = require('path')
var fs = require('graceful-fs')
var test = require('tap').test
var common = require('../common-tap.js')
var Tacks = require('tacks')
var Dir = Tacks.Dir
var File = Tacks.File

var testdir = path.join(__dirname, path.basename(__filename, '.js'))
var cwd = path.join(testdir, '3')

/**
 * NOTE: Tarball Fixtures
 * They contain package.json files with dependencies like the following:
 * 1-1.0.0.tgz: package/package.json
 * {
 *   "name":"1",
 *   "version":"1.0.0"
 * }
 * 2-1.0.0.tgz: package/package.json
 * {
 *   "name":"2",
 *   "version":"1.0.0",
 *   "dependencies":{
 *     "1":"file:../1/1-1.0.0.tgz"
 *   }
 * }
 */
var fixture = new Tacks(Dir({
  '1': Dir({
    '1-1.0.0.tgz': File(Buffer.from(
      '1f8b08000000000000032b484cce4e4c4fd52f80d07a59c5f9790c540606' +
      '06066626260ad8c4c1c0d85c81c1d8d4ccc0d0d0cccc00a80ec830353103' +
      'd2d4760836505a5c925804740aa5e640bca200a78708a856ca4bcc4d55b2' +
      '523254d2512a4b2d2acecccf03f1f40cf40c946ab906da79a360148c8251' +
      '300a6804007849dfdf00080000',
      'hex'
    ))
  }),
  '2': Dir({
    '2-1.0.0.tgz': File(Buffer.from(
      '1f8b0800000000000003ed8f3d0e83300c8599394594b90d36840cdc2602' +
      '17d19f80087468c5ddeb14a9135b91aa4af996e73c3f47f660eb8b6d291b' +
      '565567dfbb646700c0682db6fc00ea5c24456900d118e01c17a52e58f75e' +
      '648bd94f76e455befd67bd457cf44f78a64248676f242b21737908cf3b8d' +
      'beeb5d70508182d56d6820d790ab3bf2dc0a83ec62489dba2b554a6598e1' +
      'f13da1a6f62139b0a44bfaeb0b23914824b2c50b8b5b623100080000',
      'hex'
    ))
  }),
  '3': Dir({
    'package.json': File({
      name: '3',
      version: '1.0.0',
      dependencies: {
        '2': '../2/2-1.0.0.tgz'
      }
    })
  })
}))

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

test('installing local package with local dependency', function (t) {
  common.npm(
    ['install'],
    {cwd: cwd},
    function (er, code, stdout, stderr) {
      t.is(code, 0, 'no error code')
      t.is(stderr, '', 'no error output')
      t.ok(fs.existsSync(path.join(cwd, 'node_modules', '2')), 'installed direct dep')
      t.ok(fs.existsSync(path.join(cwd, 'node_modules', '1')), 'installed indirect dep')
      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
