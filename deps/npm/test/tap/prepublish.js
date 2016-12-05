// verify that prepublish runs on install, pack, and publish
var common = require('../common-tap')
var test = require('tap').test
var fs = require('graceful-fs')
var join = require('path').join
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')

var pkg = join(__dirname, 'prepublish_package')
var tmp = join(pkg, 'tmp')
var cache = join(pkg, 'cache')

test('setup', function (t) {
  var n = 0
  cleanup()
  mkdirp(pkg, then())
  mkdirp(cache, then())
  mkdirp(tmp, then())
  function then () {
    n++
    return function (er) {
      if (er) throw er
      if (--n === 0) next()
    }
  }

  function next () {
    fs.writeFile(join(pkg, 'package.json'), JSON.stringify({
      name: 'npm-test-prepublish',
      version: '1.2.5',
      scripts: { prepublish: 'echo ok' }
    }), 'ascii', function (er) {
      if (er) throw er

      t.pass('setup done')
      t.end()
    })
  }
})

test('prepublish deprecation warning on `npm pack`', function (t) {
  var env = {
    'npm_config_cache': cache,
    'npm_config_tmp': tmp,
    'npm_config_prefix': pkg,
    'npm_config_global': 'false'
  }

  for (var i in process.env) {
    if (!/^npm_config_/.test(i)) {
      env[i] = process.env[i]
    }
  }

  common.npm([
    'pack',
    '--loglevel', 'warn'
  ], { cwd: pkg, env: env }, function (err, code, stdout, stderr) {
    t.equal(code, 0, 'pack finished successfully')
    t.ifErr(err, 'pack finished successfully')

    t.match(stderr, /`prepublish` scripts will run only for `npm publish`/)
    var c = stdout.trim()
    var regex = new RegExp('' +
      '> npm-test-prepublish@1.2.5 prepublish [^\\r\\n]+\\r?\\n' +
      '> echo ok\\r?\\n' +
      '\\r?\\n' +
      'ok\\r?\\n' +
      'npm-test-prepublish-1.2.5.tgz', 'ig')

    t.match(c, regex)
    t.end()
  })
})

test('prepublish deprecation warning on `npm install`', function (t) {
  var env = {
    'npm_config_cache': cache,
    'npm_config_tmp': tmp,
    'npm_config_prefix': pkg,
    'npm_config_global': 'false'
  }

  for (var i in process.env) {
    if (!/^npm_config_/.test(i)) {
      env[i] = process.env[i]
    }
  }

  common.npm([
    'install',
    '--loglevel', 'warn'
  ], { cwd: pkg, env: env }, function (err, code, stdout, stderr) {
    t.equal(code, 0, 'pack finished successfully')
    t.ifErr(err, 'pack finished successfully')

    t.match(stderr, /`prepublish` scripts will run only for `npm publish`/)
    var c = stdout.trim()
    var regex = new RegExp('' +
      '> npm-test-prepublish@1.2.5 prepublish [^\\r\\n]+\\r?\\n' +
      '> echo ok\\r?\\n' +
      '\\r?\\n' +
      'ok' +
      '', 'ig')

    t.match(c, regex)
    t.end()
  })
})
test('cleanup', function (t) {
  cleanup()
  t.pass('cleaned up')
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}
