var common = require('../common-tap')
var test = require('tap').test
var path = require('path')
var rimraf = require('rimraf')
var osenv = require('osenv')
var mkdirp = require('mkdirp')
var pkg = path.resolve(__dirname, 'ls-depth')
var mr = require('npm-registry-mock')
var opts = {cwd: pkg}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg + '/cache')
  rimraf.sync(pkg + '/tmp')
  rimraf.sync(pkg + '/node_modules')
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg + '/cache')
  mkdirp.sync(pkg + '/tmp')
  mr({port: common.port}, function (er, s) {
    common.npm(
      [
        'install',
        '--registry', common.registry
      ],
      opts,
      function (er, c) {
        t.ifError(er, 'install ran without issue')
        t.equal(c, 0)
        s.close()
        t.end()
      }
    )
  })
})

test('npm ls --dev', function (t) {
  common.npm(['ls', '--dev'], opts, function (er, code, stdout) {
    t.ifError(er, 'ls --dev ran without issue')
    t.equal(code, 0)
    t.has(stdout, /(empty)/, 'output contains (empty)')
    t.end()
  })
})

test('npm ls --production', function (t) {
  common.npm(['ls', '--production'], opts, function (er, code, stdout) {
    t.ifError(er, 'ls --production ran without issue')
    t.notOk(code, 'npm exited ok')
    t.has(
      stdout,
      /test-package-with-one-dep@0\.0\.0/,
      'output contains test-package-with-one-dep@0.0.0'
    )
    t.end()
  })
})

test('npm ls --prod', function (t) {
  common.npm(['ls', '--prod'], opts, function (er, code, stdout) {
    t.ifError(er, 'ls --prod ran without issue')
    t.notOk(code, 'npm exited ok')
    t.has(
      stdout,
      /test-package-with-one-dep@0\.0\.0/,
      'output contains test-package-with-one-dep@0.0.0'
    )
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
