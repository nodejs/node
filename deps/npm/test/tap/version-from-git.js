var common = require('../common-tap.js')
var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../lib/npm.js')

var pkg = path.resolve(__dirname, 'version-from-git')
var packagePath = path.resolve(pkg, 'package.json')
var cache = path.resolve(pkg, 'cache')

var json = { name: 'cat', version: '0.1.2' }

test('npm version from-git with a valid tag creates a new commit', function (t) {
  var version = '1.2.3'
  setup()
  createTag(t, version, runVersion)

  function runVersion (er) {
    t.ifError(er, 'git tag ran without error')
    npm.config.set('sign-git-tag', false)
    npm.commands.version(['from-git'], checkVersion)
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
    t.ok(log.indexOf(version) !== -1, 'commit was created')
    t.end()
  }
})

test('npm version from-git with a valid tag updates the package.json version', function (t) {
  var version = '1.2.3'
  setup()
  createTag(t, version, runVersion)

  function runVersion (er) {
    t.ifError(er, 'git tag ran without error')
    npm.config.set('sign-git-tag', false)
    npm.commands.version(['from-git'], checkManifest)
  }

  function checkManifest (er) {
    t.ifError(er, 'npm run version ran without error')
    fs.readFile(path.resolve(pkg, 'package.json'), 'utf8', function (er, data) {
      t.ifError(er, 'read manifest without error')
      var manifest = JSON.parse(data)
      t.equal(manifest.version, version, 'updated the package.json version')
      t.done()
    })
  }
})

test('npm version from-git strips tag-version-prefix', function (t) {
  var version = '1.2.3'
  var prefix = 'custom-'
  var tag = prefix + version
  setup()
  createTag(t, tag, runVersion)

  function runVersion (er) {
    t.ifError(er, 'git tag ran without error')
    npm.config.set('sign-git-tag', false)
    npm.config.set('tag-version-prefix', prefix)
    npm.commands.version(['from-git'], checkVersion)
  }

  function checkVersion (er) {
    var git = require('../../lib/utils/git.js')
    t.ifError(er, 'version command ran without error')
    git.whichAndExec(
      ['log', '--pretty=medium'],
      { cwd: pkg, env: process.env },
      checkCommit
    )
  }

  function checkCommit (er, log, stderr) {
    t.ifError(er, 'git log ran without issue')
    t.notOk(stderr, 'no error output')
    t.ok(log.indexOf(tag) === -1, 'commit should not include prefix')
    t.ok(log.indexOf(version) !== -1, 'commit should include version')
    t.end()
  }
})

test('npm version from-git only strips tag-version-prefix if it is a prefix', function (t) {
  var prefix = 'test'
  var version = '1.2.3-' + prefix
  setup()
  createTag(t, version, runVersion)

  function runVersion (er) {
    t.ifError(er, 'git tag ran without error')
    npm.config.set('sign-git-tag', false)
    npm.config.set('tag-version-prefix', prefix)
    npm.commands.version(['from-git'], checkVersion)
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
    t.ok(log.indexOf(version) !== -1, 'commit should include the full version')
    t.end()
  }
})

test('npm version from-git with an existing version', function (t) {
  var tag = 'v' + json.version
  setup()
  createTag(t, tag, runVersion)

  function runVersion (er) {
    t.ifError(er, 'git tag ran without error')
    npm.config.set('sign-git-tag', false)
    npm.commands.version(['from-git'], checkVersion)
  }

  function checkVersion (er) {
    t.equal(er.message, 'Version not changed')
    t.done()
  }
})

test('npm version from-git with an invalid version tag', function (t) {
  var tag = 'invalidversion'
  setup()
  createTag(t, tag, runVersion)

  function runVersion (er) {
    t.ifError(er, 'git tag ran without error')
    npm.config.set('sign-git-tag', false)
    npm.commands.version(['from-git'], checkVersion)
  }

  function checkVersion (er) {
    t.equal(er.message, tag + ' is not a valid version')
    t.done()
  }
})

test('npm version from-git without any versions', function (t) {
  setup()
  createGitRepo(t, runVersion)

  function runVersion (er) {
    t.ifError(er, 'created git repo without errors')
    npm.config.set('sign-git-tag', false)
    npm.commands.version(['from-git'], checkVersion)
  }

  function checkVersion (er) {
    t.equal(er.message, 'No tags found')
    t.done()
  }
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  // windows fix for locked files
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

function setup () {
  cleanup()
  mkdirp.sync(cache)
  process.chdir(pkg)
  fs.writeFileSync(packagePath, JSON.stringify(json), 'utf8')
}

function createGitRepo (t, cb) {
  npm.load({ cache: cache }, function (er) {
    t.ifError(er, 'npm load ran without issue')
    common.makeGitRepo({
      path: pkg,
      added: ['package.json']
    }, cb)
  })
}

function createTag (t, tag, cb) {
  var opts = { cwd: pkg, env: { PATH: process.env.PATH } }
  npm.load({ cache: cache }, function (er) {
    t.ifError(er, 'npm load ran without issue')

    // git must be called after npm.load because it uses config
    var git = require('../../lib/utils/git.js')
    common.makeGitRepo({
      path: pkg,
      added: ['package.json'],
      commands: [git.chainableExec(['tag', tag, '-am', tag], opts)]
    }, cb)
  })
}
