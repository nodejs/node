var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var dir = path.resolve(__dirname, 'bundleddependencies')
var pkg = path.resolve(dir, 'pkg-with-bundled')
var dep = path.resolve(dir, 'a-bundled-dep')

var pj = JSON.stringify({
  name: 'pkg-with-bundled',
  version: '1.0.0',
  dependencies: {
    'a-bundled-dep': 'file:../a-bundled-dep'
  },
  bundledDependencies: {
    'a-bundled-dep': 'file:../a-bundled-dep'
  }
}, null, 2) + '\n'

var pjDep = JSON.stringify({
  name: 'a-bundled-dep',
  version: '2.0.0'
}, null, 2) + '\n'

test('setup', function (t) {
  bootstrap()
  t.end()
})

test('errors on non-array bundleddependencies', function (t) {
  t.plan(6)
  common.npm(['install'], { cwd: pkg }, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm install ran without issue')
    t.is(code, 0, 'exited with a non-error code')
    t.is(stderr, '', 'no error output')

    common.npm(['install', './pkg-with-bundled'], { cwd: dir },
      function (err, code, stdout, stderr) {
        t.ifError(err, 'npm install ran without issue')
        t.notEqual(code, 0, 'exited with a error code')
        t.like(stderr, /be an array/, 'nice error output')
      }
    )
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function bootstrap () {
  cleanup()
  mkdirp.sync(dir)
  mkdirp.sync(path.join(dir, 'node_modules'))

  mkdirp.sync(pkg)
  fs.writeFileSync(path.resolve(pkg, 'package.json'), pj)

  mkdirp.sync(dep)
  fs.writeFileSync(path.resolve(dep, 'package.json'), pjDep)
}

function cleanup () {
  rimraf.sync(dir)
}
