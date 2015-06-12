var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../')
var common = require('../common-tap.js')

// config
var pkg = path.resolve(__dirname, 'outdated')
var cache = path.resolve(pkg, 'cache')
var originalLog

var json = {
  name: 'outdated',
  description: 'fixture',
  version: '0.0.1',
  dependencies: {
    underscore: '1.3.1',
    async: '0.2.9',
    checker: '0.5.1'
  }
}

test('setup', function (t) {
  cleanup()
  originalLog = console.log
  mkdirp.sync(cache)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  process.chdir(pkg)
  t.end()
})

test('it should not throw', function (t) {
  var output = []
  var expOut = [
    path.resolve(pkg, 'node_modules', 'async') +
      ':async@0.2.9' +
      ':async@0.2.9' +
      ':async@0.2.10' +
      '\n' +
    path.resolve(pkg, 'node_modules', 'checker') +
      ':checker@0.5.1' +
      ':checker@0.5.1' +
      ':checker@0.5.2' +
      '\n' +
    path.resolve(pkg, 'node_modules', 'underscore') +
      ':underscore@1.3.1' +
      ':underscore@1.3.1' +
      ':underscore@1.5.1'
  ]

  var expData = [
    [
      pkg,
      'async',
      '0.2.9',
      '0.2.9',
      '0.2.10',
      '0.2.9'
    ],
    [
      pkg,
      'checker',
      '0.5.1',
      '0.5.1',
      '0.5.2',
      '0.5.1'
    ],
    [
      pkg,
      'underscore',
      '1.3.1',
      '1.3.1',
      '1.5.1',
      '1.3.1'
    ]
  ]

  console.log = function () {}
  mr({ port: common.port }, function (er, s) {
    npm.load(
      {
        cache: 'cache',
        loglevel: 'silent',
        parseable: true,
        registry: common.registry
      },
      function () {
        npm.install('.', function (err) {
          t.ifError(err, 'install success')
          console.log = function () {
            output.push.apply(output, arguments)
          }
          npm.outdated(function (er, d) {
            t.ifError(er, 'outdated success')

            console.log = originalLog

            t.same(output, expOut)
            t.same(d, expData)

            s.close()
            t.end()
          })
        })
      }
    )
  })
})

test('cleanup', function (t) {
  cleanup()
  console.log = originalLog
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}
