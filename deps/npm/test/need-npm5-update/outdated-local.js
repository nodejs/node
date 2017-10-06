var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var rimraf = require('rimraf')
var path = require('path')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var mkdirp = require('mkdirp')
var fs = require('graceful-fs')

var pkg = path.resolve(__dirname, 'outdated-local')
var pkgLocal = path.resolve(pkg, 'local-module')
var pkgScopedLocal = path.resolve(pkg, 'another-local-module')
var pkgLocalUnderscore = path.resolve(pkg, 'underscore')
var pkgLocalOptimist = path.resolve(pkg, 'optimist')

var pjParent = JSON.stringify({
  name: 'outdated-local',
  version: '1.0.0',
  dependencies: {
    'local-module': 'file:local-module', // updated locally, not on repo
    '@scoped/another-local-module': 'file:another-local-module', // updated locally, scoped, not on repo
    'underscore': 'file:underscore', // updated locally, updated but lesser version on repo
    'optimist': 'file:optimist' // updated locally, updated and greater version on repo
  }
}, null, 2) + '\n'

var pjLocal = JSON.stringify({
  name: 'local-module',
  version: '1.0.0'
}, null, 2) + '\n'

var pjLocalBumped = JSON.stringify({
  name: 'local-module',
  version: '1.1.0'
}, null, 2) + '\n'

var pjScopedLocal = JSON.stringify({
  name: '@scoped/another-local-module',
  version: '1.0.0'
}, null, 2) + '\n'

var pjScopedLocalBumped = JSON.stringify({
  name: '@scoped/another-local-module',
  version: '1.2.0'
}, null, 2) + '\n'

var pjLocalUnderscore = JSON.stringify({
  name: 'underscore',
  version: '1.3.1'
}, null, 2) + '\n'

var pjLocalUnderscoreBumped = JSON.stringify({
  name: 'underscore',
  version: '1.6.1'
}, null, 2) + '\n'

var pjLocalOptimist = JSON.stringify({
  name: 'optimist',
  version: '0.4.0'
}, null, 2) + '\n'

var pjLocalOptimistBumped = JSON.stringify({
  name: 'optimist',
  version: '0.5.0'
}, null, 2) + '\n'

function mocks (server) {
  server.get('/local-module')
    .reply(404)
  server.get('/@scoped%2fanother-local-module')
    .reply(404)
}

test('setup', function (t) {
  bootstrap()
  t.end()
})

test('outdated support local modules', function (t) {
  t.plan(5)
  process.chdir(pkg)
  mr({ port: common.port, plugin: mocks }, function (err, s) {
    t.ifError(err, 'mock registry started without problems')

    function verify (actual, expected) {
      for (var i = 0; i < expected.length; i++) {
        var current = expected[i]

        var found = false
        for (var j = 0; j < actual.length; j++) {
          var target = actual[j]

          var k
          for (k = 0; k < current.length; k++) {
            if (current[k] !== target[k]) break
          }
          if (k === current.length) found = true
        }

        if (!found) return false
      }

      return true
    }

    npm.load(
      {
        loglevel: 'silent',
        parseable: true,
        registry: common.registry
      },
      function () {
        npm.install('.', function (err) {
          t.ifError(err, 'install success')
          bumpLocalModules()
          npm.outdated(function (er, d) {
            t.ifError(err, 'npm outdated ran without error')
            t.is(process.exitCode, 1, 'errorCode set to 1')
            process.exitCode = 0
            t.ok(verify(d, [
              [
                path.resolve(__dirname, 'outdated-local'),
                'local-module',
                '1.0.0',
                '1.1.0',
                '1.1.0',
                'file:local-module'
              ],
              [
                path.resolve(__dirname, 'outdated-local'),
                '@scoped/another-local-module',
                '1.0.0',
                '1.2.0',
                '1.2.0',
                'file:another-local-module'
              ],
              [
                path.resolve(__dirname, 'outdated-local'),
                'underscore',
                '1.3.1',
                '1.6.1',
                '1.5.1',
                'file:underscore'
              ],
              [
                path.resolve(__dirname, 'outdated-local'),
                'optimist',
                '0.4.0',
                '0.6.0',
                '0.6.0',
                'optimist@0.6.0'
              ]
            ]), 'got expected outdated output')
            s.close()
          })
        })
      }
    )
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function bootstrap () {
  mkdirp.sync(pkg)
  fs.writeFileSync(path.resolve(pkg, 'package.json'), pjParent)

  mkdirp.sync(pkgLocal)
  fs.writeFileSync(path.resolve(pkgLocal, 'package.json'), pjLocal)

  mkdirp.sync(pkgScopedLocal)
  fs.writeFileSync(path.resolve(pkgScopedLocal, 'package.json'), pjScopedLocal)

  mkdirp.sync(pkgLocalUnderscore)
  fs.writeFileSync(path.resolve(pkgLocalUnderscore, 'package.json'), pjLocalUnderscore)

  mkdirp.sync(pkgLocalOptimist)
  fs.writeFileSync(path.resolve(pkgLocalOptimist, 'package.json'), pjLocalOptimist)
}

function bumpLocalModules () {
  fs.writeFileSync(path.resolve(pkgLocal, 'package.json'), pjLocalBumped)
  fs.writeFileSync(path.resolve(pkgScopedLocal, 'package.json'), pjScopedLocalBumped)
  fs.writeFileSync(path.resolve(pkgLocalUnderscore, 'package.json'), pjLocalUnderscoreBumped)
  fs.writeFileSync(path.resolve(pkgLocalOptimist, 'package.json'), pjLocalOptimistBumped)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
