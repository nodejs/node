var exec = require('child_process').exec
var fs = require('graceful-fs')
var path = require('path')
var existsSync = fs.existsSync || path.existsSync

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var escapeExecPath = require('../../lib/utils/escape-exec-path')

var common = require('../common-tap.js')

var pkg = common.pkg
var work = pkg + '-TEST'
var modules = path.join(work, 'node_modules')

var EXEC_OPTS = { cwd: work }

var world = '#!/usr/bin/env node\nconsole.log("hello blrbld")\n'

var json = {
  name: '@scoped/package',
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
        existsSync(path.join(modules, '@scoped', 'package', 'package.json')),
        'package installed'
      )
      t.ok(existsSync(path.join(modules, '.bin')), 'binary link directory exists')

      var hello = path.join(modules, '.bin', 'hello')
      t.ok(existsSync(hello), 'binary link exists')

      exec(escapeExecPath(hello), function (err, stdout, stderr) {
        t.ifError(err, 'command ran fine')
        t.notOk(stderr, 'got no error output back')
        t.equal(stdout, 'hello blrbld\n', 'output was as expected')

        t.end()
      })
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(work)
  rimraf.sync(pkg)
}
