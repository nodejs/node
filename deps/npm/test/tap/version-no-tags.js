var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var path = require('path')
var fs = require('fs')
var which = require('which')
var spawn = require('child_process').spawn

var pkg = common.pkg
var cache = common.cache

test('npm version <semver> without git tag', function (t) {
  setup()
  npm.load({ cache: cache, registry: common.registry }, function () {
    which('git', function (err, git) {
      t.ifError(err, 'git found on system')
      function tagExists (tag, _cb) {
        var child1 = spawn(git, ['tag', '-l', tag])
        var out = ''
        child1.stdout.on('data', function (d) {
          out += d.toString()
        })
        child1.on('exit', function () {
          return _cb(null, Boolean(~out.indexOf(tag)))
        })
      }

      var child2 = spawn(git, ['init'])
      child2.stdout.pipe(process.stdout)
      child2.on('exit', function () {
        npm.config.set('git-tag-version', false)
        npm.commands.version(['patch'], function (err) {
          if (err) return t.fail('Error perform version patch')
          var p = path.resolve(pkg, 'package')
          var testPkg = require(p)
          if (testPkg.version !== '0.0.1') t.fail(testPkg.version + ' !== \'0.0.1\'')
          t.equal('0.0.1', testPkg.version)
          tagExists('v0.0.1', function (err, exists) {
            t.ifError(err, 'tag found to exist')
            t.equal(exists, false, 'git tag DOES exist')
            t.pass('git tag does not exist')
            t.end()
          })
        })
      })
    })
  })
})

function setup () {
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Evan Lucas',
    name: 'version-no-tags-test',
    version: '0.0.0',
    description: 'Test for git-tag-version flag'
  }), 'utf8')
  process.chdir(pkg)
}
