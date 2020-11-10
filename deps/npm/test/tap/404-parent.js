var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
const pkg = common.pkg
var mr = require('npm-registry-mock')

test('404-parent: if parent exists, specify parent in error message', function (t) {
  setup()
  rimraf(path.resolve(pkg, 'node_modules'), () => {
    performInstall(function (err) {
      t.ok(err instanceof Error, 'error was returned')
      t.equal(err.parent, '404-parent', "error's parent set")
      t.end()
    })
  })
})

function setup () {
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Evan Lucas',
    name: '404-parent-test',
    version: '0.0.0',
    description: 'Test for 404-parent',
    dependencies: {
      'test-npm-404-parent-test': '*'
    }
  }), 'utf8')
  process.chdir(pkg)
}

function performInstall (cb) {
  mr({port: common.port}, function (er, s) { // create mock registry.
    if (er) {
      return cb(er)
    }
    s.get('/test-npm-404-parent-test')
      .many().reply(404, {'error': 'version not found'})
    npm.load({
      registry: common.registry
    }, function () {
      npm.config.set('fetch-retries', 0)
      var pwd = process.cwd()
      process.chdir(pkg)
      npm.commands.install([], function (err) {
        process.chdir(pwd)
        cb(err)
        s.close() // shutdown mock npm server.
      })
    })
  })
}
