var fs = require('graceful-fs')
var path = require('path')
var existsSync = fs.existsSync || path.existsSync

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var server

var pkg = path.join(__dirname, 'dedupe')

var EXEC_OPTS = { cwd: pkg }

var json = {
  author: 'Dedupe tester',
  name: 'dedupe',
  version: '0.0.0',
  dependencies: {
    optimist: '0.6.0',
    clean: '2.1.6'
  }
}

test('setup', function (t) {
  t.comment('test for https://github.com/npm/npm/issues/4675')
  setup(function () {
    t.end()
  })
})

test('dedupe finds the common module and moves it up one level', function (t) {
  common.npm(
  [
    '--registry', common.registry,
    'install', '.'
  ],
  EXEC_OPTS,
  function (err, code) {
    t.ifError(err, 'successfully installed directory')
    t.equal(code, 0, 'npm install exited with code')
    common.npm(
      [
        'dedupe'
      ],
      EXEC_OPTS,
      function (err, code) {
        t.ifError(err, 'successfully deduped against previous install')
        t.notOk(code, 'npm dedupe exited with code')

        t.ok(existsSync(path.join(pkg, 'node_modules', 'minimist')))
        t.notOk(existsSync(path.join(pkg, 'node_modules', 'checker')))

        t.end()
      }
    )
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()

  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}

function setup (cb) {
  cleanup()
  mkdirp.sync(path.join(pkg, 'node_modules'))
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)

  mr({ port: common.port }, function (er, s) {
    server = s
    cb()
  })
}
