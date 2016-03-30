var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')
var server

var pkg = path.resolve(__dirname, 'url-dependencies')

var json = {
  author: 'Steve Mason',
  name: 'url-dependencies',
  version: '0.0.0',
  dependencies: {
    underscore: common.registry + '/underscore/-/underscore-1.3.1.tgz'
  }
}

var mockRoutes = {
  'get': {
    '/underscore/-/underscore-1.3.1.tgz': [200]
  }
}

test('setup', function (t) {
  mr({ port: common.port, mocks: mockRoutes }, function (er, s) {
    server = s
    t.end()
  })
})

test('url-dependencies: download first time', function (t) {
  setup()

  performInstall(t, function (output) {
    if (!tarballWasFetched(output)) {
      t.fail('Tarball was not fetched')
    } else {
      t.pass('Tarball was fetched')
    }
    t.end()
  })
})

test('url-dependencies: do not download subsequent times', function (t) {
  setup()

  performInstall(t, function () {
    performInstall(t, function (output) {
      if (tarballWasFetched(output)) {
        t.fail('Tarball was fetched second time around')
      } else {
        t.pass('Tarball was not fetched')
      }
      t.end()
    })
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.end()
})

function cleanup () {
  // windows fix for locked files
  process.chdir(osenv.tmpdir())
  rimraf.sync(path.resolve(pkg))
}

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
}

function tarballWasFetched (output) {
  return output.indexOf(
    'http fetch GET ' +
      common.registry +
      '/underscore/-/underscore-1.3.1.tgz'
  ) > -1
}

function performInstall (t, cb) {
  var opts = {
    cwd: pkg,
    env: {
      npm_config_registry: common.registry,
      npm_config_cache_lock_stale: 1000,
      npm_config_cache_lock_wait: 1000,
      npm_config_loglevel: 'http',
      HOME: process.env.HOME,
      Path: process.env.PATH,
      PATH: process.env.PATH
    }
  }
  common.npm(['install'], opts, function (err, code, stdout, stderr) {
    t.ifError(err, 'install success')
    t.notOk(code, 'npm install exited with code 0')

    cb(stderr)
  })
}
