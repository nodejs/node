var test = require('tap').test
var path = require('path')
var fs = require('graceful-fs')

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var common = require('../common-tap.js')

var dir = path.resolve(__dirname, 'bundled-dependencies-no-pkgjson')
var pkg = path.resolve(dir, 'pkg-with-bundled-dep')
var dep = path.resolve(pkg, 'node_modules', 'a-bundled-dep')

var pkgJson = JSON.stringify({
  name: 'pkg-with-bundled-dep',
  version: '1.0.0',
  dependencies: {
  },
  bundledDependencies: [
    'a-bundled-dep'
  ]
}, null, 2) + '\n'

test('setup', function (t) {
  mkdirp.sync(path.join(dir, 'node_modules'))
  mkdirp.sync(dep)

  fs.writeFileSync(path.resolve(pkg, 'package.json'), pkgJson)
  fs.writeFileSync(path.resolve(dep, 'index.js'), '')
  t.end()
})

test('proper error on bundled dep with no package.json', function (t) {
  t.plan(3)
  var npmArgs = ['install', './' + path.basename(pkg)]

  common.npm(npmArgs, { cwd: dir }, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm ran without issue')
    t.notEqual(code, 0)
    t.like(stderr, /ENOENT/, 'ENOENT should be in stderr')
    t.end()
  })
})

test('cleanup', function (t) {
  rimraf.sync(dir)
  t.end()
})
