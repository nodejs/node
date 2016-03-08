var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = require('../../')

var pkg = path.resolve(__dirname, 'outdated-new-versions')
var cache = path.resolve(pkg, 'cache')

var json = {
  name: 'new-versions-with-outdated',
  author: 'Rockbert',
  version: '0.0.0',
  dependencies: {
    underscore: '~1.3.1'
  },
  devDependencies: {
    request: '~0.9.0'
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

test('dicovers new versions in outdated', function (t) {
  process.chdir(pkg)
  t.plan(2)

  mr({ port: common.port }, function (er, s) {
    npm.load({ cache: cache, registry: common.registry }, function () {
      npm.outdated(function (er, d) {
        for (var i = 0; i < d.length; i++) {
          if (d[i][1] === 'underscore') t.equal('1.5.1', d[i][4])
          if (d[i][1] === 'request') t.equal('2.27.0', d[i][4])
        }
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
