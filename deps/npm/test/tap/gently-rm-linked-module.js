var basename = require('path').basename
var resolve = require('path').resolve
var fs = require('graceful-fs')
var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')

var common = require('../common-tap.js')

var base = resolve(__dirname, basename(__filename, '.js'))
var pkg = resolve(base, 'gently-rm-linked')
var dep = resolve(base, 'test-linked')
var glb = resolve(base, 'test-global')
var lnk = resolve(base, 'test-global-link')

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
  // link our test module into the global folder
  common.npm(
    [
      '--prefix', lnk,
      '--loglevel', 'error',
      'link',
      dep
    ],
    EXEC_OPTS,
    function (er, code, stdout, stderr) {
      if (er) throw er
      t.is(code, 0, 'link succeeded')
      t.is(stderr, '', 'no log output')
      t.ok(doesModuleExist(), 'installed ok')

      // and try removing it and make sure that succeeds
      common.npm(
        [
          '--global',
          '--prefix', lnk,
          '--loglevel', 'error',
          'rm', '@test/linked'
        ],
        EXEC_OPTS,
        function (er, code, stdout, stderr) {
          if (er) throw er
          t.is(code, 0, 'rm succeeded')
          t.is(stderr, '', 'no log output')
          t.notOk(doesModuleExist(), 'removed ok')
          t.end()
        }
      )
    }
  )
})

test('cleanup', function (t) {
  cleanup()

  t.end()
})

function doesModuleExist () {
  var binPath = resolve(lnk, 'bin', 'linked')
  var pkgPath = resolve(lnk, 'lib', 'node_modules', '@test', 'linked')
  try {
    fs.statSync(binPath)
    fs.statSync(pkgPath)
    return true
  } catch (ex) {
    return false
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
  fs.symlinkSync(glb, lnk)
  // so it doesn't try to install into npm's own node_modules
  mkdirp.sync(resolve(pkg, 'node_modules'))
  mkdirp.sync(dep)
  fs.writeFileSync(resolve(dep, 'package.json'), JSON.stringify(fixture))
  fs.writeFileSync(resolve(dep, 'index.js'), index)
}
