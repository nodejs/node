var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = require('../../')

// config
var pkg = common.pkg
var cache = path.resolve(pkg, 'cache')

var json = {
  name: 'outdated-long',
  description: 'fixture',
  version: '0.0.1',
  dependencies: {
    underscore: '1.3.1'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(cache)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  process.chdir(pkg)
  t.end()
})

test('it should not throw', function (t) {
  var originalLog = console.log

  var output = []
  var expOut = [
    'add\tunderscore\t1.3.1\tnode_modules/underscore\t\t',
    path.resolve(pkg, 'node_modules', 'underscore') +
      ':underscore@1.3.1' +
      ':underscore@1.3.1' +
      ':underscore@1.5.1' +
      ':dependencies'
  ]

  var expData = [
    [
      pkg,
      'underscore',
      '1.3.1',
      '1.3.1',
      '1.5.1',
      '1.3.1',
      'dependencies'
    ]
  ]

  console.log = function () {
    output.push.apply(output, arguments)
  }
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
          npm.config.set('long', true)
          // since it's possible for the homepage of a package to change, after the
          // install we read the value from the package.json directly to specify our
          // expected output.
          expOut[1] = expOut[1] + ':' + JSON.parse(fs.readFileSync(path.resolve(pkg, 'node_modules', 'underscore', 'package.json'))).homepage
          npm.outdated(function (er, d) {
            t.ifError(err, 'npm outdated ran without error')
            t.is(process.exitCode, 1, 'exit code set to 1')
            process.exitCode = 0
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
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}
