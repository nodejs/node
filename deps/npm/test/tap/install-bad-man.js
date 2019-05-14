var fs = require('fs')
var resolve = require('path').resolve

var osenv = require('osenv')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = resolve(__dirname, 'install-bad-man')
var target = resolve(__dirname, 'install-bad-man-target')

var EXEC_OPTS = {
  cwd: target
}

var json = {
  name: 'install-bad-man',
  version: '1.2.3',
  man: [ './install-bad-man.1.lol' ]
}

common.pendIfWindows('man pages do not get installed on Windows')

test('setup', function (t) {
  setup()
  t.pass('setup ran')
  t.end()
})

test("install from repo on 'OS X'", function (t) {
  common.npm(
    [
      'install',
      '--prefix', target,
      '--global',
      pkg
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'npm command ran from test')
      t.equals(code, 1, 'install exited with failure (1)')
      t.notOk(stdout, 'no output indicating success')
      t.notOk(
        stderr.match(/Cannot read property '1' of null/),
        'no longer has cryptic error output'
      )
      t.ok(
        stderr.match(/install-bad-man\.1\.lol is not a valid name/),
        'got expected error output'
      )

      t.end()
    }
  )
})

test('clean', function (t) {
  cleanup()
  t.pass('cleaned up')
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  // make sure it installs locally
  mkdirp.sync(resolve(target, 'node_modules'))
  fs.writeFileSync(
    resolve(pkg, 'package.json'),
    JSON.stringify(json, null, 2) + '\n'
  )
  fs.writeFileSync(resolve(pkg, 'install-bad-man.1.lol'), 'lol\n')
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  rimraf.sync(target)
}
