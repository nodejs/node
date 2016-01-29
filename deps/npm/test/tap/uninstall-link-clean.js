var fs = require('graceful-fs')
var path = require('path')
var existsSync = fs.existsSync || path.existsSync

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.join(__dirname, 'uninstall-link-clean')
var dep = path.join(__dirname, 'dep')
var work = path.join(__dirname, 'uninstall-link-clean-TEST')
var modules = path.join(work, 'node_modules')

var EXEC_OPTS = { cwd: work }

var world = 'console.log("hello blrbld")\n'

var json = {
  name: 'package',
  version: '0.0.0',
  bin: {
    hello: './world.js'
  },
  dependencies: {
    'dep': 'file:../dep'
  }
}

var pjDep = {
  name: 'dep',
  version: '0.0.0',
  bin: {
    hello: './world.js'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  fs.writeFileSync(path.join(pkg, 'world.js'), world)

  mkdirp.sync(dep)
  fs.writeFileSync(
    path.join(dep, 'package.json'),
    JSON.stringify(pjDep, null, 2)
  )
  fs.writeFileSync(path.join(dep, 'world.js'), world)

  mkdirp.sync(modules)
  process.chdir(work)

  t.end()
})

test('installing package with links', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      'install', pkg
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'install ran to completion without error')
      t.notOk(code, 'npm install exited with code 0')

      t.ok(
        existsSync(path.join(modules, 'package', 'package.json')),
        'package installed'
      )
      t.ok(existsSync(path.join(modules, '.bin')), 'binary link directory exists')
      t.ok(existsSync(path.join(modules, 'package', 'node_modules', '.bin')),
        'nested binary link directory exists')

      t.end()
    }
  )
})

test('uninstalling package with links', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      'uninstall', 'package'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'uninstall ran to completion without error')
      t.notOk(code, 'npm uninstall exited with code 0')

      t.notOk(existsSync(path.join(modules, 'package')),
        'package directory no longer exists')
      t.notOk(existsSync(path.join(modules, 'package', 'node_modules', '.bin')),
        'nested binary link directory no longer exists')

      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(dep)
  rimraf.sync(work)
  rimraf.sync(pkg)
}
