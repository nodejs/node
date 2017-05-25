var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')

var pkg = path.resolve(__dirname, 'ls-depth-unmet')

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'ls-depth-umnet',
  author: 'Evan You',
  version: '0.0.0',
  dependencies: {
    'test-package-with-one-dep': '0.0.0',
    underscore: '1.5.1',
    optimist: '0.6.0'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        '--registry', common.registry,
        'install', '--no-save', 'underscore@1.3.1', 'mkdirp', 'test-package-with-one-dep'
      ],
      EXEC_OPTS,
      function (er, c) {
        t.ifError(er, 'setup installation ran without issue')
        t.equal(c, 0)
        s.close()
        t.end()
      }
    )
  })
})

test('npm ls --depth=0', function (t) {
  common.npm(
    ['ls', '--depth=0'],
    EXEC_OPTS,
    function (er, c, out) {
      t.ifError(er, 'setup installation ran without issue')
      t.equal(c, 1, 'ls barfed')
      t.has(
        out,
        /UNMET DEPENDENCY optimist@0\.6\.0/,
        'output contains optimist@0.6.0 and labeled as unmet dependency'
      )
      t.has(
        out,
        /mkdirp@0\.3\.5 extraneous/,
        'output contains mkdirp@0.3.5 and labeled as extraneous'
      )
      t.has(
        out,
        /underscore@1\.3\.1 invalid/,
        'output contains underscore@1.3.1 and labeled as invalid'
      )
      t.has(
        out,
        /test-package-with-one-dep@0\.0\.0\n/,
        'output contains test-package-with-one-dep@0.0.0 and has no labels'
      )
      t.doesNotHave(
        out,
        /test-package@0\.0\.0/,
        'output does not contain test-package@0.0.0 which is at depth=1'
      )
      t.end()
    }
  )
})

test('npm ls --depth=1', function (t) {
  common.npm(
    ['ls', '--depth=1'],
    EXEC_OPTS,
    function (er, c, out) {
      t.ifError(er, 'setup installation ran without issue')
      t.equal(c, 1, 'ls barfed')
      t.has(
        out,
        /UNMET DEPENDENCY optimist@0\.6\.0/,
        'output contains optimist@0.6.0 and labeled as unmet dependency'
      )
      t.has(
        out,
        /mkdirp@0\.3\.5 extraneous/,
        'output contains mkdirp@0.3.5 and labeled as extraneous'
      )
      t.has(
        out,
        /underscore@1\.3\.1 invalid/,
        'output contains underscore@1.3.1 and labeled as invalid'
      )
      t.has(
        out,
        /test-package-with-one-dep@0\.0\.0\n/,
        'output contains test-package-with-one-dep@0.0.0 and has no labels'
      )
      t.has(
        out,
        /test-package@0\.0\.0/,
        'output contains test-package@0.0.0 which is at depth=1'
      )
      t.end()
    }
  )
})

test('npm ls --depth=Infinity', function (t) {
  // travis has a preconfigured depth=0, in general we can not depend
  // on the default value in all environments, so explictly set it here
  common.npm(
    ['ls', '--depth=Infinity'],
    EXEC_OPTS,
    function (er, c, out) {
      t.ifError(er, 'setup installation ran without issue')
      t.equal(c, 1, 'ls barfed')
      t.has(
        out,
        /UNMET DEPENDENCY optimist@0\.6\.0/,
        'output contains optimist@0.6.0 and labeled as unmet dependency'
      )
      t.has(
        out,
        /mkdirp@0\.3\.5 extraneous/,
        'output contains mkdirp@0.3.5 and labeled as extraneous'
      )
      t.has(
        out,
        /underscore@1\.3\.1 invalid/,
        'output contains underscore@1.3.1 and labeled as invalid'
      )
      t.has(
        out,
        /test-package-with-one-dep@0\.0\.0\n/,
        'output contains test-package-with-one-dep@0.0.0 and has no labels'
      )
      t.has(
        out,
        /test-package@0\.0\.0/,
        'output contains test-package@0.0.0 which is at depth=1'
      )
      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
