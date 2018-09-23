var resolve = require('path').resolve
var fs = require('graceful-fs')
var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')

var common = require('../common-tap.js')

var pkg = resolve(__dirname, 'gently-rm-linked')
var dep = resolve(__dirname, 'test-linked')
var glb = resolve(__dirname, 'test-global')
var lnk = resolve(__dirname, 'test-global-link')

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

function removeBlank (line) {
  return line !== ''
}

function verify (t, stdout) {
  var binPath = resolve(lnk, 'bin', 'linked')
  var pkgPath = resolve(lnk, 'lib', 'node_modules', '@test', 'linked')
  var trgPath = resolve(pkgPath, 'index.js')
  t.deepEqual(
    stdout.split('\n').filter(removeBlank),
    [ binPath + ' -> ' + trgPath,
      resolve(lnk, 'lib'),
      '└── @test/linked@1.0.0 '
    ],
    'got expected install output'
  )
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
