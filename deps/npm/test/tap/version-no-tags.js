var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var npmc = require.resolve('../../')
var osenv = require('osenv')
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var which = require('which')
var util = require('util')
var spawn = require('child_process').spawn
var args = [ npmc
           , 'version'
           , 'patch'
           , '--no-git-tag-version'
           ]
var pkg = __dirname + '/version-no-tags'

test("npm version <semver> without git tag", function (t) {
  setup()
  npm.load({ cache: pkg + '/cache', registry: common.registry}, function () {
    which('git', function(err, git) {
      function tagExists(tag, _cb) {
        var child = spawn(git, ['tag', '-l', tag])
        var out = ''
        child.stdout.on('data', function(d) {
          out += data.toString()
        })
        child.on('exit', function() {
          return _cb(null, Boolean(~out.indexOf(tag)))
        })
      }

      var child = spawn(git, ['init'])
      child.stdout.pipe(process.stdout)
      child.on('exit', function() {
        npm.config.set('git-tag-version', false)
        npm.commands.version(['patch'], function(err) {
          if (err) return t.fail('Error perform version patch')
          var testPkg = require(pkg+'/package')
          if (testPkg.version !== '0.0.1') t.fail(testPkg.version+' !== \'0.0.1\'')
          t.ok('0.0.1' === testPkg.version)
          tagExists('v0.0.1', function(err, exists) {
            t.equal(exists, false, 'git tag DOES exist')
            t.pass('git tag does not exist')
            t.end()
          })
        })
      })
    })
  })
})

test('cleanup', function(t) {
  // windows fix for locked files
  process.chdir(osenv.tmpdir())

  rimraf.sync(pkg)
  t.end()
})

function setup() {
  mkdirp.sync(pkg)
  mkdirp.sync(pkg + '/cache')
  fs.writeFileSync(pkg + '/package.json', JSON.stringify({
    author: "Evan Lucas",
    name: "version-no-tags-test",
    version: "0.0.0",
    description: "Test for git-tag-version flag"
  }), 'utf8')
  process.chdir(pkg)
}
