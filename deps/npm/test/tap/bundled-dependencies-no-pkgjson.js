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
var packed

test('setup', function (t) {
  rimraf.sync(dir)
  mkdirp.sync(path.join(dir, 'node_modules'))
  mkdirp.sync(dep)

  fs.writeFileSync(path.resolve(pkg, 'package.json'), pkgJson)
  fs.writeFileSync(path.resolve(dep, 'index.js'), '')
  common.npm(['pack', pkg], {cwd: dir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'packed ok')
    packed = stdout.trim()
    t.comment(stderr)
    t.end()
  })
})

test('proper error on bundled dep with no package.json', function (t) {
  t.plan(2)
  var npmArgs = ['install', packed]

  common.npm(npmArgs, { cwd: dir }, function (err, code, stdout, stderr) {
    if (err) throw err
    t.notEqual(code, 0, 'npm ended in error')
    t.like(stderr, /ENOENT/, 'ENOENT should be in stderr')
    t.end()
  })
})

test('cleanup', function (t) {
  rimraf.sync(dir)
  t.end()
})
