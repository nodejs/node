var common = require('../common-tap')
  , test = require('tap').test
  , rimraf = require('rimraf')
  , npm = require('../../')
  , mr = require('npm-registry-mock')
  , pkg = __dirname + '/outdated-depth'

function cleanup () {
  rimraf.sync(pkg + '/node_modules')
  rimraf.sync(pkg + '/cache')
}

test('outdated depth integer', function (t) {
  // todo: update with test-package-with-one-dep once the new
  // npm-registry-mock is published
  var expected = [
    pkg,
    'underscore',
    '1.3.1',
    '1.3.1',
    '1.5.1',
    '1.3.1'
  ]

  process.chdir(pkg)

  mr(common.port, function (s) {
    npm.load({
      cache: pkg + '/cache'
    , loglevel: 'silent'
    , registry: common.registry
    , depth: 5
    }
    , function () {
        npm.install('request@0.9.0', function (er) {
          if (er) throw new Error(er)
          npm.outdated(function (err, d) {
            if (err) throw new Error(err)
            t.deepEqual(d[0], expected)
            s.close()
            t.end()
          })
        })
      }
    )
  })
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})