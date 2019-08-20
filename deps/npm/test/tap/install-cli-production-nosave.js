var fs = require('fs')
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

var PACKAGE_JSON1 = {
  name: 'install-cli-production-nosave',
  version: '0.0.1',
  dependencies: {
  }
}

test('setup', function (t) {
  setup()
  mr({ port: common.port }, function (er, s) {
    t.ifError(er, 'started mock registry')
    server = s
    t.end()
  })
})

test('install --production <module> without --save exits successfully', function (t) {
  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      'install', '--production', 'underscore'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')
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
    JSON.stringify(PACKAGE_JSON1, null, 2)
  )

  process.chdir(pkg)
}
