var fs = require('fs')
var resolve = require('path').resolve

var osenv = require('osenv')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../lib/npm.js')
var common = require('../common-tap.js')

var pkg = resolve(__dirname, 'add-remote-git-shrinkwrap')
var repo = resolve(__dirname, 'add-remote-git-shrinkwrap-repo')

var daemon
var daemonPID
var git

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
  npm.commands.install('.', [], function (er) {
    t.ifError(er, 'npm installed via git')

    t.end()
  })
})

test('shrinkwrap gets correct _from and _resolved (#7121)', function (t) {
  common.npm(
    [
      'shrinkwrap',
      '--loglevel', 'silent'
    ],
    { cwd: pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, 'npm shrinkwrapped without errors')
      t.notOk(code, '`npm shrinkwrap` exited with 0')
      t.equal(stdout.trim(), 'wrote npm-shrinkwrap.json')
      t.notOk(stderr, 'no error output on successful shrinkwrap')

      var shrinkwrap = require(resolve(pkg, 'npm-shrinkwrap.json'))
      t.equal(
        shrinkwrap.dependencies.child.from,
        'git://localhost:1235/child.git#master',
        'npm shrinkwrapped from correctly'
      )

      git.whichAndExec(
        ['rev-list', '-n1', 'master'],
        { cwd: repo, env: process.env },
        function (er, stdout, stderr) {
          t.ifErr(er, 'git rev-list ran without error')
          t.notOk(stderr, 'no error output')
          var treeish = stdout.trim()

          t.equal(
            shrinkwrap.dependencies.child.resolved,
            'git://localhost:1235/child.git#' + treeish,
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

var pjParent = JSON.stringify({
  name: 'parent',
  version: '1.2.3',
  dependencies: {
    'child': 'git://localhost:1235/child.git#master'
  }
}, null, 2) + '\n'

var pjChild = JSON.stringify({
  name: 'child',
  version: '1.0.3'
}, null, 2) + '\n'

function bootstrap () {
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
          '--port=1235'
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
