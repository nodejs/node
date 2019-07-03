var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var server

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'install-save-exact',
  version: '0.0.1',
  description: 'fixture'
}

test('setup', function (t) {
  setup()
  mr({ port: common.port }, function (er, s) {
    server = s
    t.end()
  })
})

test('\'npm install --save --save-exact\' should install local pkg', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      '--registry', common.registry,
      '--save',
      '--save-exact',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm ran without issue')
      t.notOk(code, 'npm install exited without raising an error code')

      var p = path.resolve(pkg, 'node_modules/underscore/package.json')
      t.ok(JSON.parse(fs.readFileSync(p)))

      p = path.resolve(pkg, 'package.json')
      var pkgJson = JSON.parse(fs.readFileSync(p, 'utf8'))

      t.same(
        pkgJson.dependencies,
        { 'underscore': '1.3.1' },
        'underscore dependency should specify exactly 1.3.1'
      )

      t.end()
    }
  )
})

test('\'npm install --save-dev --save-exact\' should install local pkg', function (t) {
  setup()

  common.npm(
    [
      '--loglevel', 'silent',
      '--registry', common.registry,
      '--save-dev',
      '--save-exact',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm ran without issue')
      t.notOk(code, 'npm install exited without raising an error code')

      var p = path.resolve(pkg, 'node_modules/underscore/package.json')
      t.ok(JSON.parse(fs.readFileSync(p)))

      p = path.resolve(pkg, 'package.json')
      var pkgJson = JSON.parse(fs.readFileSync(p, 'utf8'))

      t.same(
        pkgJson.devDependencies,
        { 'underscore': '1.3.1' },
        'underscore dependency should specify exactly 1.3.1'
      )

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

function setup () {
  cleanup()
  mkdirp.sync(path.resolve(pkg, 'node_modules'))
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)
}
