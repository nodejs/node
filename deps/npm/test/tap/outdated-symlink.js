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
var originalLog

var fakeRoot = path.join(__dirname, 'fakeRoot')
var OPTS = {
  env: {
    'npm_config_prefix': fakeRoot
  }
}

var json = {
  name: 'my-local-package',
  description: 'fixture',
  version: '1.0.0'
}

test('setup', function (t) {
  cleanup()
  originalLog = console.log
  mkdirp.sync(cache)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)
  common.npm(['install', '-g', 'async@0.2.9', 'underscore@1.3.1'], OPTS, function (err, c, out) {
    t.ifError(err, 'global install did not error')
    common.npm(['link'], OPTS, function (err, c, out) {
      t.ifError(err, 'link did not error')
      common.npm(['ls', '-g'], OPTS, function (err, c, out, stderr) {
        t.ifError(err)
        t.equal(c, 0)
        t.equal(stderr, '', 'got expected stderr')
        t.has(out, /my-local-package@1.0.0/, 'creates global link ok')
        t.end()
      })
    })
  })
})

test('when outdated is called linked packages should be displayed as such', function (t) {
  var regOutLinked = /my-local-package\s*1.0.0\s*linked\s*linked\n/
  var regOutInstallOne = /async\s*0.2.9\s*0.2.9\s*1.5.2\n/
  var regOutInstallTwo = /underscore\s*1.3.1\s*1.3.1\s*1.8.3\n/

  console.log = function () {}
  mr({ port: common.port }, function (er, s) {
    common.npm(['outdated', '-g'], OPTS, function (err, c, out, stderr) {
      t.ifError(err)
      t.ok(out.match(regOutLinked), 'Global Link format as expected')
      t.ok(out.match(regOutInstallOne), 'Global Install format as expected')
      t.ok(out.match(regOutInstallTwo), 'Global Install format as expected')
      s.close()
      t.end()
    })
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  common.npm(['rm', 'outdated'], OPTS, function (err, code) {
    t.ifError(err, 'npm removed the linked package without error')
    t.equal(code, 0, 'cleanup outdated in local ok')
    common.npm(['rm', '-g', 'outdated', 'async', 'underscore'], OPTS, function (err, code) {
      t.ifError(err, 'npm removed the global package without error')
      t.equal(code, 0, 'cleanup outdated in global ok')

      console.log = originalLog
      cleanup()
      t.end()
    })
  })
})

function cleanup () {
  rimraf.sync(pkg)
  rimraf.sync(fakeRoot)
}
