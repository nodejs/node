var fs = require('graceful-fs')
var path = require('path')
var osenv = require('osenv')
var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'outdated-symlink')
var cache = path.resolve(pkg, 'cache')
var extend = Object.assign || require('util')._extend

var fakeRoot = path.join(pkg, 'fakeRoot')
var OPTS = {
  env: extend(extend({}, process.env), {
    'npm_config_prefix': fakeRoot,
    'registry': common.registry
  })
}

var json = {
  name: 'my-local-package',
  description: 'fixture',
  version: '1.0.0'
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(cache)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)
  common.npm(['install', '-g', 'async@0.2.9', 'underscore@1.3.1'], OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout)
    t.comment(stderr)
    common.npm(['link'], OPTS, function (err, code, stdout, stderr) {
      if (err) throw err
      t.comment(stdout)
      t.comment(stderr)
      t.is(code, 0)
      common.npm(['ls', '-g'], OPTS, function (err, code, stdout, stderr) {
        if (err) throw err
        t.is(code, 0)
        t.is(stderr, '', 'got expected stderr')
        t.match(stdout, /my-local-package@1.0.0/, 'creates global link ok')
        t.end()
      })
    })
  })
})

test('when outdated is called linked packages should be displayed as such', function (t) {
  var regOutLinked = /my-local-package\s*1.0.0\s*linked\s*linked\n/
  var regOutInstallOne = /async\s*0.2.9\s*0.2.9\s*0.2.10\n/
  var regOutInstallTwo = /underscore\s*1.3.1\s*1.3.1\s*1.5.1\n/

  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        '--registry', common.registry,
        'outdated', '-g'
      ],
      OPTS,
      function (err, c, out, stderr) {
        if (err) throw err
        t.is(stderr, '')
        t.match(out, regOutLinked, 'Global Link format as expected')
        t.match(out, regOutInstallOne, 'Global Install format as expected')
        t.match(out, regOutInstallTwo, 'Global Install format as expected')
        s.close()
        t.end()
      }
    )
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  common.npm(['rm', 'outdated'], OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout)
    t.comment(stderr)
    t.is(code, 0, 'cleanup outdated in local ok')
    common.npm(['rm', '-g', 'outdated', 'async', 'underscore'], OPTS, function (err, code) {
      if (err) throw err
      t.comment(stdout)
      t.comment(stderr)
      t.is(code, 0, 'cleanup outdated in global ok')

      cleanup()
      t.end()
    })
  })
})

function cleanup () {
  rimraf.sync(pkg)
  rimraf.sync(fakeRoot)
}
