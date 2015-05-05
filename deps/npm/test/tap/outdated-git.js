var fs = require('graceful-fs')
var path = require('path')

var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')

var common = require('../common-tap.js')
var npm = require('../../')

// config
var pkg = path.resolve(__dirname, 'outdated-git')
var cache = path.resolve(pkg, 'cache')
var json = {
  name: 'outdated-git',
  author: 'Rocko Artischocko',
  description: 'fixture',
  version: '0.0.1',
  main: 'index.js',
  dependencies: {
    'foo-private': 'git://github.com/robertkowalski/foo-private.git',
    'foo-private-credentials': 'git://user:pass@github.com/robertkowalski/foo-private.git',
    'foo-github': 'robertkowalski/foo'
  }
}

test('setup', function (t) {
  setup()
  t.end()
})

test('discovers new versions in outdated', function (t) {
  process.chdir(pkg)
  t.plan(5)
  npm.load({cache: cache, registry: common.registry, loglevel: 'silent'}, function () {
    npm.commands.outdated([], function (er, d) {
      t.equal(d[0][3], 'git')
      t.equal(d[0][4], 'git')
      t.equal(d[0][5], 'git://github.com/robertkowalski/foo-private.git')
      t.equal(d[1][5], 'git://user:pass@github.com/robertkowalski/foo-private.git')
      t.equal(d[2][5], 'github:robertkowalski/foo')
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  mkdirp.sync(cache)
  fs.writeFileSync(path.join(pkg, 'package.json'), JSON.stringify(json, null, 2), 'utf8')
}

function cleanup () {
  rimraf.sync(pkg)
}
