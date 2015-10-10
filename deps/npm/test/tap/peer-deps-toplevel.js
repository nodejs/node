var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = npm = require('../../')

var pkg = path.resolve(__dirname, 'peer-deps-toplevel')

var expected = {
  name: 'npm-test-peer-deps-toplevel',
  version: '0.0.0',
  problems: [
    'peer dep missing: mkdirp@*, required by npm-test-peer-deps-toplevel@0.0.0',
    'peer dep missing: request@0.9.x, required by npm-test-peer-deps@0.0.0'
  ],
  dependencies: {
    'npm-test-peer-deps': {
      version: '0.0.0',
      from: 'npm-test-peer-deps@*',
      resolved: common.registry + '/npm-test-peer-deps/-/npm-test-peer-deps-0.0.0.tgz',
      dependencies: {
        underscore: {
          version: '1.3.1',
          from: 'underscore@1.3.1',
          resolved: common.registry + '/underscore/-/underscore-1.3.1.tgz'
        }
      }
    },
    mkdirp: {
      peerMissing: true,
      required: {
        _id: 'mkdirp@*',
        name: 'mkdirp',
        version: '*',
        peerMissing: [
          {requiredBy: 'npm-test-peer-deps-toplevel@0.0.0', requires: 'mkdirp@*'}
        ],
        dependencies: {}
      }
    },
    request: {
      peerMissing: true,
      required: {
        _id: 'request@0.9.x',
        dependencies: {},
        name: 'request',
        peerMissing: [
          {requiredBy: 'npm-test-peer-deps@0.0.0', requires: 'request@0.9.x'}
        ],
        version: '0.9.x'
      }
    }
  }
}

var json = {
  author: 'Domenic Denicola',
  name: 'npm-test-peer-deps-toplevel',
  version: '0.0.0',
  dependencies: {
    'npm-test-peer-deps': '*'
  },
  peerDependencies: {
    mkdirp: '*'
  }
}

test('installs the peer dependency directory structure', function (t) {
  mr({ port: common.port }, function (er, s) {
    setup(function (err) {
      t.ifError(err, 'setup ran successfully')

      npm.install('.', function (err) {
        t.ifError(err, 'packages were installed')

        npm.commands.ls([], true, function (err, _, results) {
          t.ifError(err, 'listed tree without problems')

          t.deepEqual(results, expected, 'got expected output from ls')
          s.close()
          t.end()
        })
      })
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup (cb) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)

  var opts = { cache: path.resolve(pkg, 'cache'), registry: common.registry}
  npm.load(opts, cb)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
