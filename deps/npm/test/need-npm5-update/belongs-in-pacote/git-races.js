/* eslint-disable camelcase */
var execFile = require('child_process').execFile
var path = require('path')
var zlib = require('zlib')

var asyncMap = require('slide').asyncMap
var deepEqual = require('deep-equal')
var fs = require('graceful-fs')
var mkdirp = require('mkdirp')
var once = require('once')
var requireInject = require('require-inject')
var rimraf = require('rimraf')
var tar = require('tar')
var test = require('tap').test
var tmpdir = require('osenv').tmpdir
var which = require('which')

var wd = path.resolve(tmpdir(), 'git-races')
var fixtures = path.resolve(__dirname, '../fixtures')
var testcase = 'github-com-BryanDonovan-npm-git-test'
var testcase_git = path.resolve(wd, testcase + '.git')
var testcase_path = path.resolve(wd, testcase)
var testcase_tgz = path.resolve(fixtures, testcase + '.git.tar.gz')

var testtarballs = []
var testrepos = {}
var testurls = {}

/*
This test is specifically for #7202, where the bug was if you tried installing multiple git urls that
pointed at the same repo but had different comittishes, you'd sometimes get the wrong version.
The test cases, provided by @BryanDonovan, have a dependency tree like this:

  top
    bar#4.0.0
      buzz#3.0.0
      foo#3.0.0
    buzz#3.0.0
    foo#4.0.0
      buzz#2.0.0

But what would happen is that buzz#2.0.0 would end up installed under bar#4.0.0.

bar#4.0.0 shouldn't have gotten its own copy if buzz, and if it did, it shouldn've been buzz#3.0.0
*/

;['bar', 'foo', 'buzz'].forEach(function (name) {
  var mockurl = 'ssh://git@github.com/BryanDonovan/dummy-npm-' + name + '.git'
  var realrepo = path.resolve(wd, 'github-com-BryanDonovan-dummy-npm-' + name + '.git')
  var tgz = path.resolve(fixtures, 'github-com-BryanDonovan-dummy-npm-' + name + '.git.tar.gz')

  testrepos[mockurl] = realrepo
  testtarballs.push(tgz)
})

function cleanup () {
  process.chdir(tmpdir())
  rimraf.sync(wd)
}

var npm = requireInject.installGlobally('../../lib/npm.js', {
  'child_process': {
    'execFile': function (cmd, args, options, cb) {
      // on win 32, the following prefix is added in utils/git.js
      // $ git -c core.longpaths=true clone
      var i = process.platform === 'win32' ? 2 : 0
      // If it's a clone we swap any requests for any of the urls we're mocking
      // with the path to the bare repo
      if (args[i] === 'clone') {
        var m2 = args.length - 2
        var m1 = args.length - 1
        if (testrepos[args[m2]]) {
          testurls[args[m1]] = args[m2]
          args[m2] = testrepos[args[m2]]
        }
        execFile(cmd, args, options, cb)
      // here, we intercept npm validating the remote origin url on one of the
      // clones we've done previously and return the original url that was requested
      } else if (args[i] === 'config' && args[i + 1] === '--get' && args[i + 2] === 'remote.origin.url') {
        process.nextTick(function () {
          cb(null, testurls[options.cwd], '')
        })
      } else {
        execFile(cmd, args, options, cb)
      }
    }
  }
})

function extract (tarball, target, cb) {
  cb = once(cb)
  fs.createReadStream(tarball).on('error', function (er) { cb(er) })
    .pipe(zlib.createGunzip()).on('error', function (er) { cb(er) })
    .pipe(tar.Extract({path: target})).on('error', function (er) { cb(er) })
    .on('end', function () {
      cb()
    })
}

// Copied from lib/utils/git, because we need to use
// it before calling npm.load and lib/utils/git uses npm.js
// which doesn't allow that. =( =(

function prefixGitArgs () {
  return process.platform === 'win32' ? ['-c', 'core.longpaths=true'] : []
}

var gitcmd

function execGit (args, options, cb) {
  var fullArgs = prefixGitArgs().concat(args || [])
  return execFile(gitcmd, fullArgs, options, cb)
}

function gitWhichAndExec (args, options, cb) {
  if (gitcmd) return execGit(args, options, cb)

  which('git', function (err, pathtogit) {
    if (err) {
      err.code = 'ENOGIT'
      return cb(err)
    }
    gitcmd = pathtogit

    execGit(args, options, cb)
  })
}

function andClone (gitdir, repodir, cb) {
  return function (er) {
    if (er) return cb(er)
    gitWhichAndExec(['clone', gitdir, repodir], {}, cb)
  }
}

function setup (cb) {
  cleanup()
  mkdirp.sync(wd)

  extract(testcase_tgz, wd, andClone(testcase_git, testcase_path, andExtractPackages))

  function andExtractPackages (er) {
    if (er) return cb(er)
    asyncMap(testtarballs, function (tgz, done) {
      extract(tgz, wd, done)
    }, andChdir)
  }
  function andChdir (er) {
    if (er) return cb(er)
    process.chdir(testcase_path)
    andLoadNpm()
  }
  function andLoadNpm () {
    var opts = {
      cache: path.resolve(wd, 'cache')
    }
    npm.load(opts, cb)
  }
}

// there are two (sic) valid trees that can result we don't care which one we
// get in npm@2
var oneTree = [
  'npm-git-test@1.0.0', [
    ['dummy-npm-bar@4.0.0', [
      ['dummy-npm-foo@3.0.0', []],
      ['dummy-npm-buzz@3.0.0', []]
    ]],
    ['dummy-npm-buzz@3.0.0', []],
    ['dummy-npm-foo@4.0.0', [
      ['dummy-npm-buzz@2.0.0', []]
    ]]
  ]
]
var otherTree = [
  'npm-git-test@1.0.0', [
    ['dummy-npm-bar@4.0.0', [
      ['dummy-npm-buzz@3.0.0', []],
      ['dummy-npm-foo@3.0.0', []]
    ]],
    ['dummy-npm-buzz@3.0.0', []],
    ['dummy-npm-foo@4.0.0', [
      ['dummy-npm-buzz@2.0.0', []]
    ]]
  ]
]

function toSimple (tree) {
  var deps = []
  Object.keys(tree.dependencies || {}).forEach(function (dep) {
    deps.push(toSimple(tree.dependencies[dep]))
  })
  return [ tree['name'] + '@' + tree['version'], deps ]
}

test('setup', function (t) {
  setup(function (er) {
    t.ifError(er, 'setup ran OK')
    t.end()
  })
})

test('correct versions are installed for git dependency', function (t) {
  t.comment('test for https://github.com/npm/npm/issues/7202')
  npm.commands.install([], function (er) {
    t.ifError(er, 'installed OK')
    npm.commands.ls([], true, function (er, result) {
      t.ifError(er, 'ls OK')
      var simplified = toSimple(result)
      if (deepEqual(simplified, oneTree) || deepEqual(simplified, otherTree)) {
        t.pass('install tree is correct')
      } else {
        t.isDeeply(simplified, oneTree, 'oneTree matches')
        t.isDeeply(simplified, otherTree, 'otherTree matches')
      }
      t.done()
    })
  })
})
