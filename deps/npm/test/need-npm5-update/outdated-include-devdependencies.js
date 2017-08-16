var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../')
var common = require('../common-tap.js')

// config
var pkg = path.resolve(__dirname, 'outdated-include-devdependencies')
var cache = path.resolve(pkg, 'cache')

var json = {
  author: 'Rocko Artischocko',
  name: 'ignore-shrinkwrap',
  version: '0.0.0',
  devDependencies: {
    underscore: '>=1.3.1'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(cache)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  t.end()
})

test('includes devDependencies in outdated', function (t) {
  process.chdir(pkg)
  mr({ port: common.port }, function (er, s) {
    npm.load({ cache: cache, registry: common.registry }, function () {
      npm.outdated(function (er, d) {
        t.ifError(er, 'npm outdated completed successfully')
        t.is(process.exitCode, 1, 'exitCode set to 1')
        process.exitCode = 0
        t.equal('1.5.1', d[0][3])
        s.close()
        t.end()
      })
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}
