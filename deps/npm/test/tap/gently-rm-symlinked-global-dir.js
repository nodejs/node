var common = require('../common-tap.js')

var resolve = require('path').resolve
var fs = require('graceful-fs')
var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var isWindows = require('../../lib/utils/is-windows.js')

var pkg = resolve(common.pkg, 'package')
var dep = resolve(common.pkg, 'test-linked')
var glb = resolve(common.pkg, 'test-global')
var lnk = resolve(common.pkg, 'test-global-link')

var EXEC_OPTS = { cwd: pkg }

var index = "module.exports = function () { console.log('whoop whoop') }"

var fixture = {
  name: '@test/linked',
  version: '1.0.0',
  bin: {
    linked: './index.js'
  }
}

test('setup', function (t) {
  cleanup()
  setup()

  t.end()
})

test('install and link', function (t) {
  common.npm(
    [
      '--global',
      '--prefix', lnk,
      '--loglevel', 'silent',
      '--json',
      'install', '../test-linked'
    ],
    EXEC_OPTS,
    function (er, code, stdout, stderr) {
      t.ifError(er, "test-linked install didn't explode")
      t.notOk(code, 'test-linked install also failed')
      t.notOk(stderr, 'no log output')

      verify(t, stdout)

      // again, to make sure unlinking works properlyt
      common.npm(
        [
          '--global',
          '--prefix', lnk,
          '--loglevel', 'silent',
          '--json',
          'install', '../test-linked'
        ],
        EXEC_OPTS,
        function (er, code, stdout, stderr) {
          t.ifError(er, "test-linked install didn't explode")
          t.notOk(code, 'test-linked install also failed')
          t.notOk(stderr, 'no log output')

          verify(t, stdout)

          fs.readdir(pkg, function (er, files) {
            t.ifError(er, 'package directory is still there')
            t.deepEqual(files, ['node_modules'], 'only stub modules dir remains')

            t.end()
          })
        }
      )
    }
  )
})

test('cleanup', function (t) {
  cleanup()

  t.end()
})

function resolvePath () {
  return resolve.apply(null, Array.prototype.slice.call(arguments)
    .filter(function (arg) { return arg }))
}

function verify (t, stdout) {
  var result = JSON.parse(stdout)
  var pkgPath = resolvePath(lnk, !isWindows && 'lib', 'node_modules', '@test', 'linked')
  if (result.added.length) {
    t.is(result.added.length, 1, 'added the module')
    t.is(result.added[0].path, pkgPath, 'in the right location')
  } else {
    t.is(result.updated.length, 1, 'updated the module')
    t.is(result.updated[0].path, pkgPath, 'in the right location')
  }
}

function cleanup () {
  rimraf.sync(pkg)
  rimraf.sync(dep)
  rimraf.sync(lnk)
  rimraf.sync(glb)
}

function setup () {
  mkdirp.sync(pkg)
  mkdirp.sync(glb)
  fs.symlinkSync(glb, lnk, 'junction')
  // so it doesn't try to install into npm's own node_modules
  mkdirp.sync(resolve(pkg, 'node_modules'))
  mkdirp.sync(dep)
  fs.writeFileSync(resolve(dep, 'package.json'), JSON.stringify(fixture))
  fs.writeFileSync(resolve(dep, 'index.js'), index)
}
