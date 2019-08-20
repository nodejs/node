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
  name: 'install-noargs-dev',
  version: '0.0.1',
  devDependencies: {
    'underscore': '1.3.1'
  }
}

var PACKAGE_JSON2 = {
  name: 'install-noargs-dev',
  version: '0.0.2',
  devDependencies: {
    'underscore': '1.5.1'
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

test('install noargs installs devDependencies', function (t) {
  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--no-save',
      'install'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')

      var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
      var pkgJson = JSON.parse(fs.readFileSync(p))

      t.equal(pkgJson.version, '1.3.1')
      t.end()
    }
  )
})

test('install noargs installs updated devDependencies', function (t) {
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(PACKAGE_JSON2, null, 2)
  )

  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--no-save',
      'install'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')

      var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
      var pkgJson = JSON.parse(fs.readFileSync(p))

      t.equal(pkgJson.version, '1.5.1')
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
