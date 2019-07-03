var fs = require('fs')
var resolve = require('path').resolve

var osenv = require('osenv')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../lib/npm.js')
var common = require('../common-tap.js')

var pkg = common.pkg
var repo = pkg + '-repo'

var daemon
var daemonPID
var git

var pjParent = JSON.stringify({
  name: 'parent',
  version: '1.2.3',
  dependencies: {
    'child': 'git://localhost:' + common.gitPort + '/child.git#master'
  }
}, null, 2) + '\n'

var pjChild = JSON.stringify({
  name: 'child',
  version: '1.0.3'
}, null, 2) + '\n'

test('setup', function (t) {
  bootstrap()
  setup(function (er, r) {
    t.ifError(er, 'git started up successfully')

    if (!er) {
      daemon = r[r.length - 2]
      daemonPID = r[r.length - 1]
    }

    t.end()
  })
})

test('install from repo', function (t) {
  process.chdir(pkg)
  common.npm(['install'], {cwd: pkg, stdio: [0, 1, 2]}, function (er, code) {
    if (er) throw er
    t.is(code, 0, 'npm installed via git')

    t.end()
  })
})

test('shrinkwrap gets correct _from and _resolved (#7121)', function (t) {
  common.npm(
    [
      'shrinkwrap',
      '--loglevel', 'error'
    ],
    { cwd: pkg, stdio: [0, 'pipe', 2] },
    function (er, code, stdout) {
      if (er) throw er
      t.is(code, 0, '`npm shrinkwrap` exited ok')

      var shrinkwrap = require(resolve(pkg, 'npm-shrinkwrap.json'))
      git.whichAndExec(
        ['rev-list', '-n1', 'master'],
        { cwd: repo, env: process.env },
        function (er, stdout, stderr) {
          t.ifErr(er, 'git rev-list ran without error')
          t.notOk(stderr, 'no error output')
          var treeish = stdout.trim()

          t.like(shrinkwrap, {dependencies: {child: {version: 'git://localhost:' + common.gitPort + '/child.git#' + treeish}}},
            'npm shrinkwrapped resolved correctly'
          )

          t.end()
        }
      )
    }
  )
})

test('clean', function (t) {
  daemon.on('close', function () {
    cleanup()
    t.end()
  })
  process.kill(daemonPID)
})

function bootstrap () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(resolve(pkg, 'package.json'), pjParent)
}

function setup (cb) {
  mkdirp.sync(repo)
  fs.writeFileSync(resolve(repo, 'package.json'), pjChild)
  npm.load({ prefix: pkg, registry: common.registry, loglevel: 'silent' }, function () {
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

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(repo)
  rimraf.sync(pkg)
}
