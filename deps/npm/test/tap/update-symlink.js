'use strict'
const path = require('path')
const test = require('tap').test
const mr = require('npm-registry-mock')
const Tacks = require('tacks')
const File = Tacks.File
const Symlink = Tacks.Symlink
const Dir = Tacks.Dir
const common = require('../common-tap.js')

const basedir = path.join(__dirname, path.basename(__filename, '.js'))
const testdir = path.join(basedir, 'testdir')
const cachedir = path.join(basedir, 'cache')
const globaldir = path.join(basedir, 'global')
const tmpdir = path.join(basedir, 'tmp')

const conf = {
  cwd: path.join(testdir, 'main'),
  env: Object.assign({}, process.env, {
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
}

let server
const fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    broken: Dir({
      'package.json': File({
        name: 'broken',
        version: '1.0.0'
      })
    }),
    main: Dir({
      node_modules: Dir({
        unbroken: Symlink('/testdir/unbroken')
      }),
      'package-lock.json': File({
        name: 'main',
        version: '1.0.0',
        lockfileVersion: 1,
        requires: true,
        dependencies: {
          broken: {
            version: 'file:../broken'
          },
          unbroken: {
            version: 'file:../unbroken'
          }
        }
      }),
      'package.json': File({
        name: 'main',
        version: '1.0.0',
        dependencies: {
          broken: 'file:../broken',
          unbroken: 'file:../unbroken'
        }
      })
    }),
    unbroken: Dir({
      'package.json': File({
        name: 'unbroken',
        version: '1.0.0'
      })
    })
  })
}))

function setup () {
  cleanup()
  fixture.create(basedir)
}

function cleanup () {
  fixture.remove(basedir)
}

test('setup', function (t) {
  setup()
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    t.done()
  })
})

test('update fixes broken links', function (t) {
  common.npm(['update'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.match(stdout, '+ broken@1.0.0')
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
