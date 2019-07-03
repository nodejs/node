var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var osenv = require('osenv')
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var which = require('which')
var spawn = require('child_process').spawn

var pkg = common.pkg
var cache = path.resolve(pkg, 'cache')

test('npm version <semver> with working directory not clean', function (t) {
  setup()
  npm.load({ cache: cache, registry: common.registry, prefix: pkg }, function () {
    which('git', function (err, git) {
      t.ifError(err, 'git found')

      function addPackageJSON (_cb) {
        var data = JSON.stringify({ name: 'blah', version: '0.1.2' })
        fs.writeFile('package.json', data, function () {
          var child = spawn(git, ['add', 'package.json'])
          child.on('exit', function () {
            var child2 = spawn(git, ['commit', 'package.json', '-m', 'init'])
            var out = ''
            child2.stdout.on('data', function (d) {
              out += d.toString()
            })
            child2.on('exit', function () {
              return _cb(out)
            })
          })
        })
      }

      common.makeGitRepo({path: pkg}, function () {
        addPackageJSON(function () {
          var data = JSON.stringify({ name: 'blah', version: '0.1.3' })
          fs.writeFile('package.json', data, function () {
            npm.commands.version(['patch'], function (err) {
              if (!err) {
                t.fail('should fail on non-clean working directory')
              } else {
                t.ok(err.message.match(/Git working directory not clean./))
                t.ok(err.message.match(/M package.json/))
              }
              t.end()
            })
          })
        })
      })
    })
  })
})

test('npm version <semver> --force with working directory not clean', function (t) {
  common.npm(
    [
      '--force',
      '--no-sign-git-commit',
      '--no-sign-git-tag',
      '--registry', common.registry,
      '--prefix', pkg,
      'version',
      'patch'
    ],
    { cwd: pkg, env: {PATH: process.env.PATH} },
    function (err, code, stdout, stderr) {
      t.ifError(err, 'npm version ran without issue')
      t.notOk(code, 'exited with a non-error code')
      var errorLines = stderr.trim().split('\n')
        .map(function (line) {
          return line.trim()
        })
        .filter(function (line) {
          return !line.indexOf('using --force')
        })
      t.notOk(errorLines.length, 'no error output')
      t.end()
    })
})

test('cleanup', function (t) {
  // windows fix for locked files
  process.chdir(osenv.tmpdir())

  rimraf.sync(pkg)
  t.end()
})

function setup () {
  mkdirp.sync(pkg)
  mkdirp.sync(cache)
  process.chdir(pkg)
}
