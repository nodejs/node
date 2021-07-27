'use strict'

const fs = require('fs')
const path = require('path')

const test = require('tap').test
const mr = require('npm-registry-mock')

const npm = require('../../lib/npm.js')
const common = require('../common-tap.js')

const testdir = common.pkg
const repo = path.join(testdir, 'repo')
const prefix = path.join(testdir, 'prefix')
const cache = common.cache

var Tacks = require('tacks')
var Dir = Tacks.Dir
var File = Tacks.File

let daemon
let daemonPID
let git
let mockRegistry

process.env.npm_config_prefix = prefix

const fixture = new Tacks(Dir({
  repo: Dir({}),
  prefix: Dir({}),
  deps: Dir({
    parent: Dir({
      'package.json': File({
        name: 'parent',
        version: '1.2.3',
        dependencies: {
          'child': 'git://localhost:' + common.gitPort + '/child.git'
        }
      })
    }),
    child: Dir({
      'package.json': File({
        name: 'child',
        version: '1.0.3',
        main: 'dobuild.js',
        scripts: {
          'prepublish': 'exit 123',
          'prepare': 'writer build-artifact'
        },
        devDependencies: {
          writer: 'file:' + path.join(testdir, 'deps', 'writer')
        }
      })
    }),
    writer: Dir({
      'package.json': File({
        name: 'writer',
        version: '1.0.0',
        bin: 'writer.js'
      }),
      'writer.js': File(`#!/usr/bin/env node\n
        require('fs').writeFileSync(process.argv[2], 'hello, world!')
        `)
    })
  })
}))

test('setup', function (t) {
  fixture.create(testdir)
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

test('install from git repo with prepare script', function (t) {
  const parent = path.join(testdir, 'deps', 'parent')

  common.npm([
    'install',
    '--no-save',
    '--registry', common.registry,
    '--cache', cache,
    '--loglevel', 'error'
  ], {
    cwd: parent
  }, function (err, code, stdout, stderr) {
    if (err) { throw err }
    t.equal(code, 0, 'exited successfully')
    t.equal(stderr, '', 'no actual output on stderr')

    const target = path.join(parent, 'node_modules', 'child', 'build-artifact')
    fs.readFile(target, 'utf8', (err, data) => {
      if (err) { throw err }
      t.equal(data, 'hello, world!', 'build artifact written for git dep')
      t.end()
    })
  })
})

test('clean', function (t) {
  mockRegistry.close()
  daemon.on('close', t.end)
  process.kill(daemonPID)
})

function setup (cb) {
  npm.load({
    prefix: testdir,
    loglevel: 'silent'
  }, function () {
    git = require('../../lib/utils/git.js')

    function startDaemon (cb) {
      // start git server
      const d = git.spawn(
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
          cwd: repo,
          env: process.env
        }
      )
      d.stderr.on('data', childFinder)

      function childFinder (c) {
        const cpid = c.toString().match(/^\[(\d+)\]/)
        if (cpid[1]) {
          this.removeListener('data', childFinder)
          cb(null, [d, cpid[1]])
        }
      }
    }

    const childPath = path.join(testdir, 'deps', 'child')
    common.makeGitRepo({
      path: childPath,
      commands: [
        git.chainableExec([
          'clone', '--bare', childPath, 'child.git'
        ], { cwd: repo, env: process.env }),
        startDaemon
      ]
    }, cb)
  })
}
