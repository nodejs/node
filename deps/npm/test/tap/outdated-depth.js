var common = require('../common-tap')
  , path = require('path')
  , test = require('tap').test
  , rimraf = require('rimraf')
  , npm = require('../../')
  , mr = require('npm-registry-mock')
  , pkg = path.resolve(__dirname, 'outdated-depth')

function cleanup () {
  rimraf.sync(pkg + '/node_modules')
  rimraf.sync(pkg + '/cache')
}

test('outdated depth zero', function (t) {
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
    , depth: 0
    }
    , function () {
        npm.install('.', function (er) {
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
