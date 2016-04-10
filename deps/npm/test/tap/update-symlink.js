var fs = require('graceful-fs')
var path = require('path')
var osenv = require('osenv')
var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'update-symlink')
var originalLog

var fakeRoot = path.join(__dirname, 'fakeRoot')
var OPTS = {
  env: {
    'npm_config_prefix': fakeRoot
  }
}

var jsonLocal = {
  name: 'my-local-package',
  description: 'fixture',
  version: '1.0.0',
  dependencies: {
    'async': '*',
    'underscore': '*'
  }
}

test('setup', function (t) {
  cleanup()
  originalLog = console.log
  mkdirp.sync(pkg)
  common.npm(['install', '-g', 'underscore@1.3.1'], OPTS, function (err, c, out) {
    t.ifError(err, 'global install did not error')
    process.chdir(pkg)
    fs.writeFileSync(
      path.join(pkg, 'package.json'),
      JSON.stringify(jsonLocal, null, 2)
    )
    common.npm(['link', 'underscore'], OPTS, function (err, c, out) {
      t.ifError(err, 'link did not error')
      common.npm(['install', 'async@0.2.9'], OPTS, function (err, c, out) {
        t.ifError(err, 'local install did not error')
        common.npm(['ls'], OPTS, function (err, c, out, stderr) {
          t.ifError(err)
          t.equal(c, 0)
          t.equal(stderr, '', 'got expected stderr')
          t.has(out, /async@0.2.9/, 'installed ok')
          t.has(out, /underscore@1.3.1/, 'creates local link ok')
          t.end()
        })
      })
    })
  })
})

test('when update is called linked packages should be excluded', function (t) {
  console.log = function () {}
  mr({ port: common.port }, function (er, s) {
    common.npm(['update'], OPTS, function (err, c, out, stderr) {
      t.ifError(err)
      t.has(out, /async@1.5.2/, 'updated ok')
      t.doesNotHave(stderr, /ERR!/, 'no errors in stderr')
      s.close()
      t.end()
    })
  })
})

test('cleanup', function (t) {
  common.npm(['rm', 'underscore', 'async'], OPTS, function (err, code) {
    t.ifError(err, 'npm removed the linked package without error')
    t.equal(code, 0, 'cleanup in local ok')
    process.chdir(osenv.tmpdir())
    common.npm(['rm', '-g', 'underscore'], OPTS, function (err, code) {
      t.ifError(err, 'npm removed the global package without error')
      t.equal(code, 0, 'cleanup in global ok')

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
