var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../')
var common = require('../common-tap')

var pkg = path.resolve(__dirname, 'peer-deps-invalid')
var cache = path.resolve(pkg, 'cache')

var json = {
  author: 'Domenic Denicola <domenic@domenicdenicola.com> (http://domenicdenicola.com/)',
  name: 'peer-deps-invalid',
  version: '0.0.0',
  dependencies: {
    'npm-test-peer-deps-file': 'http://localhost:' + common.port + '/ok.js',
    'npm-test-peer-deps-file-invalid': 'http://localhost:' + common.port + '/invalid.js'
  }
}

var fileFail = function () {
/**package
* { "name": "npm-test-peer-deps-file-invalid"
* , "main": "index.js"
* , "version": "1.2.3"
* , "description":"This one should conflict with the other one"
* , "peerDependencies": { "underscore": "1.3.3" }
* }
**/
  module.exports = 'I\'m just a lonely index, naked as the day I was born.'
}.toString().split('\n').slice(1, -1).join('\n')

var fileOK = function () {
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
  cleanup()
  mkdirp.sync(cache)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  fs.writeFileSync(path.join(pkg, 'file-ok.js'), fileOK)
  fs.writeFileSync(path.join(pkg, 'file-fail.js'), fileFail)

  process.chdir(pkg)
  t.end()
})

test('installing dependencies that have conflicting peerDependencies', function (t) {
  var customMocks = {
    'get': {
      '/ok.js': [200, path.join(pkg, 'file-ok.js')],
      '/invalid.js': [200, path.join(pkg, 'file-fail.js')]
    }
  }
  mr({port: common.port, mocks: customMocks}, function (err, s) {
    t.ifError(err, 'mock registry started')
    npm.load(
      {
        cache: cache,
        registry: common.registry
      },
      function () {
        npm.commands.install([], function (err, additions, tree) {
          t.error(err)
          var invalid = tree.warnings.filter(function (warning) { return warning.code === 'EPEERINVALID' })
          t.is(invalid.length, 2)
          s.close()
          t.end()
        })
      }
    )
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
