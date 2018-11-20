var fs = require('graceful-fs')
var path = require('path')
var existsSync = fs.existsSync || path.existsSync

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var server

var pkg = path.resolve(__dirname, 'circular-dep')
var minimist = path.join(pkg, 'minimist')

var EXEC_OPTS = {
  cwd: path.join(pkg, 'minimist/node_modules'),
  npm_config_cache: path.join(pkg, 'cache')
}

var json = {
  name: 'minimist',
  version: '0.0.5',
  dependencies: {
    optimist: '0.6.0'
  }
}

test('setup', function (t) {
  t.comment('test for https://github.com/npm/npm/issues/4312')
  setup(function () {
    t.end()
  })
})

test('installing a package that depends on the current package', function (t) {
  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      'install', 'optimist'
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'npm ran without issue')
      t.notOk(code, 'npm ran without raising an error code')
      t.notOk(stderr, 'no error output')

      common.npm(
        [
          '--registry', common.registry,
          '--loglevel', 'silent',
          'dedupe'
        ],
        EXEC_OPTS,
        function (err, code, stdout, stderr) {
          t.ifError(err, 'npm ran without issue')
          t.notOk(code, 'npm ran without raising an error code')
          t.notOk(stderr, 'no error output')

          t.ok(existsSync(path.resolve(
            minimist,
            'node_modules', 'optimist',
            'node_modules', 'minimist'
          )), 'circular dependency uncircled')
          t.end()
        }
      )
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  server.close()
  t.end()
})

function setup (cb) {
  cleanup()
  mkdirp.sync(minimist)
  fs.writeFileSync(
    path.join(minimist, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(path.resolve(pkg, 'minimist'))

  fs.mkdirSync(path.resolve(pkg, 'minimist/node_modules'))
  mr({ port: common.port }, function (er, s) {
    server = s
    cb()
  })
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
