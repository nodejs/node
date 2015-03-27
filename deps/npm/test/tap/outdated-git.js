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

test('setup', function (t) {
  setup()
  t.end()
})

test('discovers new versions in outdated', function (t) {
  process.chdir(pkg)
  t.plan(5)
  npm.load({cache: cache, registry: common.registry, loglevel: 'silent'}, function () {
    npm.commands.outdated([], function (er, d) {
      t.equal('git', d[0][3])
      t.equal('git', d[0][4])
      t.equal('git://github.com/robertkowalski/foo-private.git', d[0][5])
      t.equal('git://user:pass@github.com/robertkowalski/foo-private.git', d[1][5])
      t.equal('git+https://github.com/robertkowalski/foo', d[2][5])
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

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

function setup () {
  mkdirp.sync(cache)
  fs.writeFileSync(path.join(pkg, 'package.json'), JSON.stringify(json, null, 2), 'utf8')
}

function cleanup () {
  rimraf.sync(pkg)
}
