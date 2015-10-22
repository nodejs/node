var common = require('../common-tap')
  , test = require('tap').test
  , rimraf = require('rimraf')
  , npm = require('../../')
  , mr = require('npm-registry-mock')
  , pkg = __dirname + '/outdated-depth-integer'

var osenv = require("osenv")
var mkdirp = require("mkdirp")
var fs = require("fs")

var pj = JSON.stringify({
  "name": "whatever",
  "description": "yeah idk",
  "version": "1.2.3",
  "main": "index.js",
  "dependencies": {
    "underscore": "1.3.1"
  },
  "repository": "git://github.com/luk-/whatever"
}, null, 2);

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

function setup () {
  mkdirp.sync(pkg)
  process.chdir(pkg)
  fs.writeFileSync("package.json", pj)
}

test("setup", function (t) {
  cleanup()
  setup()
  t.end()
})

test('outdated depth integer', function (t) {
  // todo: update with test-package-with-one-dep once the new
  // npm-registry-mock is published
  var expected = [[
    pkg,
    'underscore',
    undefined, // no version installed
    '1.3.1',   // wanted
    '1.5.1',   // latest
    '1.3.1'
  ]]

  mr({port : common.port}, function (er, s) {
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
            t.deepEqual(d, expected)
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
