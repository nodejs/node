var common = require('../common-tap.js')
var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test

var npm = require('../../lib/npm.js')

var pkg = common.pkg
var subDirectory = path.resolve(pkg, 'sub-directory')
var packagePath = path.resolve(pkg, 'package.json')
var cache = common.cache

var json = { name: 'cat', version: '0.1.2' }

test('npm version <semver> from a subdirectory', function (t) {
  mkdirp.sync(subDirectory)
  process.chdir(subDirectory)
  fs.writeFileSync(packagePath, JSON.stringify(json), 'utf8')
  npmLoad()

  function npmLoad () {
    npm.load({ cache: cache }, function () {
      common.makeGitRepo({
        path: pkg,
        added: ['package.json']
      }, version)
    })
  }

  function version (er, stdout, stderr) {
    t.ifError(er, 'git repo initialized without issue')
    t.notOk(stderr, 'no error output')
    npm.config.set('sign-git-commit', false)
    npm.config.set('sign-git-tag', false)
    npm.commands.version(['patch'], checkVersion)
  }

  function checkVersion (er) {
    var git = require('../../lib/utils/git.js')
    t.ifError(er, 'version command ran without error')
    git.whichAndExec(
      ['log'],
      { cwd: pkg, env: process.env },
      checkCommit
    )
  }

  function checkCommit (er, log, stderr) {
    t.ifError(er, 'git log ran without issue')
    t.notOk(stderr, 'no error output')
    t.ok(log.match(/0\.1\.3/g), 'commited from subdirectory')
    t.end()
  }
})
