var fs = require('fs')
var resolve = require('path').resolve

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test
var readJson = require('read-package-json')
var mr = require('npm-registry-mock')

var npm = require('../../lib/npm.js')
var common = require('../common-tap.js')

var pkg = resolve(common.pkg, 'package')
var repo = resolve(common.pkg, 'repo')
var prefix = resolve(common.pkg, 'prefix')
var cache = common.cache

var daemon
var daemonPID
var git
var mockRegistry

var EXEC_OPTS = {
  registry: common.registry,
  cwd: pkg,
  cache: cache
}
process.env.npm_config_prefix = prefix

var pjParent = JSON.stringify({
  name: 'parent',
  version: '1.2.3',
  dependencies: {
    'child': 'git://localhost:' + common.gitPort + '/child.git'
  }
}, null, 2) + '\n'

var pjChild = JSON.stringify({
  name: 'child',
  version: '1.0.3'
}, null, 2) + '\n'

test('setup', function (t) {
  t.test('bootstrap', t => bootstrap(t.end))
  t.test('setup', t => setup(function (er, r) {
    t.ifError(er, 'git started up successfully')

    if (!er) {
      daemon = r[r.length - 2]
      daemonPID = r[r.length - 1]
    }

    mr({
      port: common.port
    }, function (er, server) {
      t.ifError(er, 'started mock registry')
      mockRegistry = server

      t.end()
    })
  }))
  t.end()
})

test('install from git repo [no --link]', function (t) {
  process.chdir(pkg)

  common.npm(['install'], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm install failed')

    t.dissimilar(stderr, /Command failed:/, 'expect git to succeed')
    t.dissimilar(stderr, /version not found/, 'should not go to repository')

    readJson(resolve(pkg, 'node_modules', 'child', 'package.json'), function (err, data) {
      t.ifError(err, 'error reading child package.json')

      t.equal(data && data.version, '1.0.3')
      t.end()
    })
  })
})

test('install from git repo [with --link]', function (t) {
  process.chdir(pkg)
  rimraf.sync(resolve(pkg, 'node_modules'))

  common.npm(['install', '--link'], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm install --link failed')
    t.equal(code, 0, 'npm install --link returned non-0 code')

    t.dissimilar(stderr, /Command failed:/, 'expect git to succeed')
    t.dissimilar(stderr, /version not found/, 'should not go to repository')
    t.equal(stderr, '', 'no actual output on stderr')

    readJson(resolve(pkg, 'node_modules', 'child', 'package.json'), function (err, data) {
      t.ifError(err, 'error reading child package.json')

      t.equal(data && data.version, '1.0.3')
      t.end()
    })
  })
})

test('clean', function (t) {
  mockRegistry.close()
  daemon.on('close', t.end)
  process.kill(daemonPID)
})

function bootstrap (cb) {
  rimraf(repo, () => {
    rimraf(pkg, () => {
      mkdirp.sync(pkg)
      mkdirp.sync(cache)

      fs.writeFileSync(resolve(pkg, 'package.json'), pjParent)
      cb()
    })
  })
}

function setup (cb) {
  mkdirp.sync(repo)
  fs.writeFileSync(resolve(repo, 'package.json'), pjChild)
  npm.load({
    link: true,
    prefix: pkg,
    loglevel: 'silent'
  }, function () {
    git = require('../../lib/utils/git.js')

    function startDaemon (cb) {
      // start git server
      var d = git.spawn(
        [
          'daemon',
          '--verbose',
          '--listen=localhost',
          '--export-all',
          '--base-path=.',
          '--reuseaddr',
          '--port=' + common.gitPort
        ],
        {
          cwd: pkg,
          env: process.env,
          stdio: ['pipe', 'pipe', 'pipe']
        }
      )
      d.stderr.on('data', childFinder)

      function childFinder (c) {
        var cpid = c.toString().match(/^\[(\d+)\]/)
        if (cpid[1]) {
          this.removeListener('data', childFinder)
          cb(null, [d, cpid[1]])
        }
      }
    }

    common.makeGitRepo({
      path: repo,
      commands: [
        git.chainableExec(
          ['clone', '--bare', repo, 'child.git'],
          { cwd: pkg, env: process.env }
        ),
        startDaemon
      ]
    }, cb)
  })
}
