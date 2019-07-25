var fs = require('fs')
var path = require('path')

var test = require('tap').test

var npm = require('../../')
var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

test('npm version --preid=rc uses prerelease id', function (t) {
  setup()

  npm.load({ cache: pkg + '/cache', registry: common.registry }, function () {
    common.npm(['version', 'prerelease', '--preid=rc'], EXEC_OPTS, function (err) {
      if (err) return t.fail('Error perform version prerelease')
      var newVersion = require(path.resolve(pkg, 'package.json')).version
      t.equal(newVersion, '0.0.1-rc.0', 'got expected version')
      t.end()
    })
  })
})

function setup () {
  var contents = {
    author: 'Daniel Wilches',
    name: 'version-prerelease-id',
    version: '0.0.0',
    description: 'Test for version of prereleases with preids'
  }

  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify(contents), 'utf8')
  fs.writeFileSync(path.resolve(pkg, 'npm-shrinkwrap.json'), JSON.stringify(contents), 'utf8')
  process.chdir(pkg)
}
