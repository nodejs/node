var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')

var pkg = path.resolve(__dirname, 'ls-depth')

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'ls-env',
  version: '0.0.0',
  dependencies: {
    'test-package-with-one-dep': '0.0.0'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  mr({port: common.port}, function (er, s) {
    common.npm(
      [
        '--registry', common.registry,
        'install'
      ],
      EXEC_OPTS,
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
  common.npm(['ls', '--dev'], EXEC_OPTS, function (er, code, stdout) {
    t.ifError(er, 'ls --dev ran without issue')
    t.equal(code, 0)
    t.has(stdout, /(empty)/, 'output contains (empty)')
    t.end()
  })
})

test('npm ls --only=development', function (t) {
  common.npm(['ls', '--only=development'], EXEC_OPTS, function (er, code, stdout) {
    t.ifError(er, 'ls --only=development ran without issue')
    t.equal(code, 0)
    t.has(stdout, /(empty)/, 'output contains (empty)')
    t.end()
  })
})

test('npm ls --only=dev', function (t) {
  common.npm(['ls', '--only=dev'], EXEC_OPTS, function (er, code, stdout) {
    t.ifError(er, 'ls --only=dev ran without issue')
    t.equal(code, 0)
    t.has(stdout, /(empty)/, 'output contains (empty)')
    t.end()
  })
})

test('npm ls --production', function (t) {
  common.npm(['ls', '--production'], EXEC_OPTS, function (er, code, stdout) {
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
  common.npm(['ls', '--prod'], EXEC_OPTS, function (er, code, stdout) {
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

test('npm ls --only=production', function (t) {
  common.npm(['ls', '--only=production'], EXEC_OPTS, function (er, code, stdout) {
    t.ifError(er, 'ls --only=production ran without issue')
    t.notOk(code, 'npm exited ok')
    t.has(
      stdout,
      /test-package-with-one-dep@0\.0\.0/,
      'output contains test-package-with-one-dep@0.0.0'
    )
    t.end()
  })
})

test('npm ls --only=prod', function (t) {
  common.npm(['ls', '--only=prod'], EXEC_OPTS, function (er, code, stdout) {
    t.ifError(er, 'ls --only=prod ran without issue')
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

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
