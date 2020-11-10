'use strict'

var fs = require('fs')
var resolve = require('path').resolve
var url = require('url')

var mkdirp = require('mkdirp')
var test = require('tap').test

var npm = require('../../lib/npm.js')
var fetchPackageMetadata = require('../../lib/fetch-package-metadata.js')
var common = require('../common-tap.js')

var pkg = resolve(common.pkg, 'package')
var repo = resolve(common.pkg, 'repo')
mkdirp.sync(pkg)

var git
var cloneURL = 'git+file://' + resolve(pkg, 'child.git')

var pjChild = JSON.stringify({
  name: 'child',
  version: '1.0.3'
}, null, 2) + '\n'

test('setup', function (t) {
  setup(function (er, r) {
    t.ifError(er, 'git started up successfully')

    t.end()
  })
})

test('cache from repo', function (t) {
  process.chdir(pkg)
  fetchPackageMetadata(cloneURL, process.cwd(), {}, (err, manifest) => {
    if (err) t.fail(err.message)
    t.equal(
      url.parse(manifest._resolved).protocol,
      'git+file:',
      'npm didn\'t go crazy adding git+git+git+git'
    )
    t.equal(manifest._requested.type, 'git', 'cached git')
    t.end()
  })
})

test('save install', function (t) {
  process.chdir(pkg)
  fs.writeFileSync('package.json', JSON.stringify({
    name: 'parent',
    version: '5.4.3'
  }, null, 2) + '\n')
  var prev = npm.config.get('save')
  npm.config.set('save', true)
  npm.commands.install('.', [cloneURL], function (er) {
    npm.config.set('save', prev)
    t.ifError(er, 'npm installed via git')
    var pj = JSON.parse(fs.readFileSync('package.json', 'utf-8'))
    var dep = pj.dependencies.child
    t.equal(
      url.parse(dep).protocol,
      'git+file:',
      'npm didn\'t strip the git+ from git+file://'
    )

    t.end()
  })
})

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
