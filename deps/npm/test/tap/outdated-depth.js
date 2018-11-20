var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../')
var common = require('../common-tap')

var pkg = path.resolve(__dirname, 'outdated-depth')

var json = {
  name: 'outdated-depth',
  version: '1.2.3',
  dependencies: {
    underscore: '1.3.1',
    'npm-test-peer-deps': '0.0.0'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)

  t.end()
})

test('outdated depth zero', function (t) {
  var expected = [
    pkg,
    'underscore',
    '1.3.1',
    '1.3.1',
    '1.5.1',
    '1.3.1',
    null
  ]

  mr({ port: common.port }, function (er, s) {
    npm.load(
      {
        loglevel: 'silent',
        registry: common.registry
      },
      function () {
        npm.install('.', function (er) {
          if (er) throw new Error(er)
          npm.outdated(function (err, d) {
            t.ifError(err, 'npm outdated ran without error')
            t.is(process.exitCode, 1, 'exit code set to 1')
            process.exitCode = 0
            t.deepEqual(d[0], expected)
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
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
