var fs = require('graceful-fs')
var path = require('path')

var mr = require('npm-registry-mock')
var test = require('tap').test

var common = require('../common-tap')

var pkg = common.pkg

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

const tarballWasFetched = output => output.includes(
  `GET 200 ${common.registry}/underscore/-/underscore-1.3.1.tgz`)

const performInstall = () => common.npm(['install'], {
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
})

test('setup', function (t) {
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  mr({ port: common.port, mocks: mockRoutes }, function (er, s) {
    t.parent.teardown(() => s.close())
    t.end()
  })
})

test('url-dependencies: download first time', t =>
  performInstall().then(([code, _, output]) => {
    t.equal(code, 0, 'exited successfully')
    t.ok(tarballWasFetched(output), 'download first time')
  })
    .then(() => performInstall()).then(([code, _, output]) => {
      t.equal(code, 0, 'exited successfully')
      t.notOk(tarballWasFetched(output), 'do not download second time')
    }))
