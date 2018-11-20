var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var server

var pkg = path.join(__dirname, 'install-save-prefix')

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'install-save-prefix',
  version: '0.0.1'
}

test('setup', function (t) {
  setup()
  mr({ port: common.port }, function (er, s) {
    t.ifError(er, 'started mock registry')
    server = s
    t.end()
  })
})

test('install --save with \'^\' save prefix should accept minor updates', function (t) {
  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--save-prefix', '^',
      '--save',
      'install', 'underscore@latest'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')

      var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
      t.ok(JSON.parse(fs.readFileSync(p)))

      var pkgJson = JSON.parse(fs.readFileSync(
        path.join(pkg, 'package.json'),
        'utf8'
      ))
      t.deepEqual(
        pkgJson.dependencies,
        { 'underscore': '^1.5.1' },
        'got expected save prefix and version of 1.5.1'
      )
      t.end()
    }
  )
})

test('install --save-dev with \'^\' save prefix should accept minor dev updates', function (t) {
  setup()
  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--save-prefix', '^',
      '--save-dev',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')

      var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
      t.ok(JSON.parse(fs.readFileSync(p)))

      var pkgJson = JSON.parse(fs.readFileSync(
        path.join(pkg, 'package.json'),
        'utf8'
      ))
      t.deepEqual(
        pkgJson.devDependencies,
        { 'underscore': '^1.3.1' },
        'got expected save prefix and version of 1.3.1'
      )
      t.end()
    }
  )
})

test('install --save with \'~\' save prefix should accept patch updates', function (t) {
  setup()
  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--save-prefix', '~',
      '--save',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')

      var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
      t.ok(JSON.parse(fs.readFileSync(p)))

      var pkgJson = JSON.parse(fs.readFileSync(
        path.join(pkg, 'package.json'),
        'utf8'
      ))
      t.deepEqual(
        pkgJson.dependencies,
        { 'underscore': '~1.3.1' },
        'got expected save prefix and version of 1.3.1'
      )
      t.end()
    }
  )
})

test('install --save-dev with \'~\' save prefix should accept patch updates', function (t) {
  setup()
  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--save-prefix', '~',
      '--save-dev',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')

      var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
      t.ok(JSON.parse(fs.readFileSync(p)))

      var pkgJson = JSON.parse(fs.readFileSync(
        path.join(pkg, 'package.json'),
        'utf8'
      ))
      t.deepEqual(
        pkgJson.devDependencies,
        { 'underscore': '~1.3.1' },
        'got expected save prefix and version of 1.3.1'
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
