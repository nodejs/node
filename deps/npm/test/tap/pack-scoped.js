// verify that prepublish runs on pack and publish
var test = require('tap').test
var common = require('../common-tap')
var fs = require('graceful-fs')
var join = require('path').join
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')

var pkg = common.pkg
var manifest = join(pkg, 'package.json')
var tmp = join(pkg, 'tmp')
var cache = common.cache

var data = {
  name: '@scope/generic-package',
  version: '90000.100001.5'
}

test('setup', function (t) {
  var n = 0

  rimraf.sync(pkg)

  mkdirp(pkg, then())
  mkdirp(cache, then())
  mkdirp(tmp, then())

  function then () {
    n++
    return function (er) {
      t.ifError(er)
      if (--n === 0) next()
    }
  }

  function next () {
    fs.writeFile(manifest, JSON.stringify(data), 'ascii', done)
  }

  function done (er) {
    t.ifError(er)

    t.pass('setup done')
    t.end()
  }
})

test('test', function (t) {
  var env = {
    'npm_config_cache': cache,
    'npm_config_tmp': tmp,
    'npm_config_prefix': pkg,
    'npm_config_global': 'false'
  }

  for (var i in process.env) {
    if (!/^npm_config_/.test(i)) env[i] = process.env[i]
  }

  common.npm([
    'pack',
    '--loglevel', 'warn'
  ], {
    cwd: pkg,
    env: env
  }, function (err, code, stdout, stderr) {
    t.ifErr(err, 'npm pack finished without error')
    t.equal(code, 0, 'npm pack exited ok')
    t.notOk(stderr, 'got stderr data: ' + JSON.stringify('' + stderr))
    stdout = stdout.trim()
    var regex = new RegExp('scope-generic-package-90000.100001.5.tgz', 'ig')
    t.ok(stdout.match(regex), 'found package')
    t.end()
  })
})

test('cleanup', function (t) {
  rimraf.sync(pkg)
  t.pass('cleaned up')
  t.end()
})
