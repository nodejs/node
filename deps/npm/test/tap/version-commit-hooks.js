var fs = require('graceful-fs')
var path = require('path')
var osenv = require('osenv')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
const common = require('../common-tap.js')
var pkg = common.pkg

var test = require('tap').test
var npm = require('../../')

delete process.env['npm_config_commit_hooks']

test('npm version <semver> with commit-hooks disabled in .npmrc', function (t) {
  mkdirp.sync(pkg)
  var npmrc = path.resolve(pkg, '.npmrc')
  fs.writeFileSync(npmrc, 'commit-hooks=false\n', 'ascii')
  process.chdir(pkg)

  npm.load({ prefix: pkg, userconfig: npmrc }, function (err, conf) {
    if (err) {
      t.fail('error loading npm')
    }
    t.same(npm.config.get('commit-hooks'), false)
    t.end()
  })
})

test('npm version <semver> with commit-hooks disabled', function (t) {
  npm.load({}, function () {
    npm.config.set('commit-hooks', false)

    var version = require('../../lib/version')
    var args1 = version.buildCommitArgs()
    var args2 = version.buildCommitArgs([ 'commit' ])
    var args3 = version.buildCommitArgs([ 'commit', '-m', 'some commit message' ])

    t.same(args1, [ 'commit', '-n' ])
    t.same(args2, [ 'commit', '-n' ])
    t.same(args3, [ 'commit', '-m', 'some commit message', '-n' ])
    t.end()
  })
})

test('npm version <semver> with commit-hooks enabled (default)', function (t) {
  npm.load({}, function () {
    npm.config.set('commit-hooks', true)

    var version = require('../../lib/version')
    var args1 = version.buildCommitArgs()
    var args2 = version.buildCommitArgs([ 'commit' ])
    var args3 = version.buildCommitArgs([ 'commit', '-m', 'some commit message' ])

    t.same(args1, [ 'commit' ])
    t.same(args2, [ 'commit' ])
    t.same(args3, [ 'commit', '-m', 'some commit message' ])
    t.end()
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  t.end()
})
