var fs = require('fs')
var resolve = require('path').resolve
var url = require('url')

var osenv = require('osenv')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../lib/npm.js')
var common = require('../common-tap.js')

var pkg = resolve(__dirname, 'add-remote-git-file')
var repo = resolve(__dirname, 'add-remote-git-file-repo')

var git
var cloneURL = 'git+file://' + resolve(pkg, 'child.git')

var pjChild = JSON.stringify({
  name: 'child',
  version: '1.0.3'
}, null, 2) + '\n'

test('setup', function (t) {
  bootstrap()
  setup(function (er, r) {
    t.ifError(er, 'git started up successfully')

    t.end()
  })
})

test('cache from repo', function (t) {
  process.chdir(pkg)
  var addRemoteGit = require('../../lib/cache/add-remote-git.js')
  addRemoteGit(cloneURL, function (er, data) {
    t.ifError(er, 'cached via git')
    t.equal(
      url.parse(data._resolved).protocol,
      'git+file:',
      'npm didn\'t go crazy adding git+git+git+git'
    )

    t.end()
  })
})

test('clean', function (t) {
  cleanup()
  t.end()
})

function bootstrap () {
  cleanup()
  mkdirp.sync(pkg)
}

function setup (cb) {
  mkdirp.sync(repo)
  fs.writeFileSync(resolve(repo, 'package.json'), pjChild)
  npm.load({ registry: common.registry, loglevel: 'silent' }, function () {
    git = require('../../lib/utils/git.js')

    common.makeGitRepo({
      path: repo,
      commands: [git.chainableExec(
        ['clone', '--bare', repo, 'child.git'],
        { cwd: pkg, env: process.env }
      )]
    }, cb)
  })
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(repo)
  rimraf.sync(pkg)
}
