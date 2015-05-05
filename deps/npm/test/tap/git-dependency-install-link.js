var fs = require('fs')
var resolve = require('path').resolve

var osenv = require('osenv')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test
var readJson = require('read-package-json')
var mr = require('npm-registry-mock')

var npm = require('../../lib/npm.js')
var common = require('../common-tap.js')

var pkg = resolve(__dirname, 'git-dependency-install-link')
var repo = resolve(__dirname, 'git-dependency-install-link-repo')
var cache = resolve(pkg, 'cache')

var daemon
var daemonPID
var git
var mockRegistry

var EXEC_OPTS = {
  registry: common.registry,
  cwd: pkg,
  cache: cache
}

test('setup', function (t) {
  bootstrap()
  setup(function (er, r) {
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
  })
})

test('install from git repo [no --link]', function (t) {
  process.chdir(pkg)

  common.npm(['install', '--loglevel', 'error'], EXEC_OPTS, function (err, code, stdout, stderr) {
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

  common.npm(['install', '--link', '--loglevel', 'error'], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifError(err, 'npm install --link failed')

    t.dissimilar(stderr, /Command failed:/, 'expect git to succeed')
    t.dissimilar(stderr, /version not found/, 'should not go to repository')

    readJson(resolve(pkg, 'node_modules', 'child', 'package.json'), function (err, data) {
      t.ifError(err, 'error reading child package.json')

      t.equal(data && data.version, '1.0.3')
      t.end()
    })
  })
})

test('clean', function (t) {
  mockRegistry.close()
  daemon.on('close', function () {
    cleanup()
    t.end()
  })
  process.kill(daemonPID)
})

var pjParent = JSON.stringify({
  name: 'parent',
  version: '1.2.3',
  dependencies: {
    'child': 'git://localhost:1234/child.git'
  }
}, null, 2) + '\n'

var pjChild = JSON.stringify({
  name: 'child',
  version: '1.0.3'
}, null, 2) + '\n'

function bootstrap () {
  rimraf.sync(repo)
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  mkdirp.sync(cache)

  fs.writeFileSync(resolve(pkg, 'package.json'), pjParent)
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
            '--port=1234'
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

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(repo)
  rimraf.sync(pkg)
}
