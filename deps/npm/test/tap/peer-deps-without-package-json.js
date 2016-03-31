var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../')
var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'peer-deps-without-package-json')
var cache = path.resolve(pkg, 'cache')
var nodeModules = path.resolve(pkg, 'node_modules')

var fileJS = function () {
/**package
* { "name": "npm-test-peer-deps-file"
* , "main": "index.js"
* , "version": "1.2.3"
* , "description":"No package.json in sight!"
* , "peerDependencies": { "underscore": "1.3.1" }
* , "dependencies": { "mkdirp": "0.3.5" }
* }
**/

  module.exports = 'I\'m just a lonely index, naked as the day I was born.'
}.toString().split('\n').slice(1, -1).join('\n')

test('setup', function (t) {
  t.comment('test for https://github.com/npm/npm/issues/3049')
  cleanup()
  mkdirp.sync(cache)
  mkdirp.sync(nodeModules)
  fs.writeFileSync(path.join(pkg, 'file-js.js'), fileJS)
  process.chdir(pkg)

  t.end()
})

test('installing a peerDeps-using package without package.json', function (t) {
  var customMocks = {
    'get': {
      '/ok.js': [200, path.join(pkg, 'file-js.js')]
    }
  }
  mr({port: common.port, mocks: customMocks}, function (err, s) {
    t.ifError(err, 'mock registry booted')
    npm.load({
      registry: common.registry,
      cache: cache
    }, function () {
      npm.install(common.registry + '/ok.js', function (err) {
        t.ifError(err, 'installed ok.js')

        t.ok(
          fs.existsSync(path.join(nodeModules, 'npm-test-peer-deps-file')),
          'passive peer dep installed'
        )
        t.ok(
          fs.existsSync(path.join(nodeModules, 'underscore')),
          'underscore installed'
        )

        t.end()
        s.close() // shutdown mock registry.
      })
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
