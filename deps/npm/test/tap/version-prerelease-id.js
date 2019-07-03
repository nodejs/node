var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../')
var common = require('../common-tap.js')

var pkg = common.pkg
var cache = path.resolve(pkg, 'cache')

var EXEC_OPTS = { cwd: pkg }

test('setup', function (t) {
  setup()
  t.end()
})

test('npm version --preid=rc uses prerelease id', function (t) {
  setup()

  npm.load({ cache: pkg + '/cache', registry: common.registry }, function () {
    common.npm(['version', 'prerelease', '--preid=rc'], EXEC_OPTS, function (err) {
      if (err) return t.fail('Error perform version prerelease')
      var newVersion = require(path.resolve(pkg, 'package.json')).version
      t.equal(newVersion, '0.0.1-rc.0', 'got expected version')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  mkdirp.sync(cache)
  var contents = {
    author: 'Daniel Wilches',
    name: 'version-prerelease-id',
    version: '0.0.0',
    description: 'Test for version of prereleases with preids'
  }

  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify(contents), 'utf8')
  fs.writeFileSync(path.resolve(pkg, 'npm-shrinkwrap.json'), JSON.stringify(contents), 'utf8')
  process.chdir(pkg)
}

function cleanup () {
  // windows fix for locked files
  process.chdir(osenv.tmpdir())
  rimraf.sync(cache)
  rimraf.sync(pkg)
}
