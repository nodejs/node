'use strict'

const BB = require('bluebird')

const fs = require('graceful-fs')
const path = require('path')

const mkdirp = require('mkdirp')
const osenv = require('osenv')
const requireInject = require('require-inject')
const rimraf = require('rimraf')
const test = require('tap').test

const common = require('../common-tap.js')

const pkg = common.pkg

const json = {
  name: 'github-shortcut',
  version: '0.0.0'
}

test('setup', function (t) {
  setup()
  t.end()
})

test('github-shortcut', function (t) {
  const cloneUrls = [
    ['git://github.com/foo/private.git', 'GitHub shortcuts try git URLs first'],
    ['https://github.com/foo/private.git', 'GitHub shortcuts try HTTPS URLs second'],
    ['ssh://git@github.com/foo/private.git', 'GitHub shortcuts try SSH third']
  ]
  const npm = requireInject.installGlobally('../../lib/npm.js', {
    'pacote/lib/util/git': {
      'revs': (repo, opts) => {
        return BB.resolve().then(() => {
          const cloneUrl = cloneUrls.shift()
          if (cloneUrl) {
            t.is(repo, cloneUrl[0], cloneUrl[1])
          } else {
            t.fail('too many attempts to clone')
          }
          throw new Error('git.revs mock fails on purpose')
        })
      }
    }
  })

  const opts = {
    cache: path.resolve(pkg, 'cache'),
    prefix: pkg,
    registry: common.registry,
    loglevel: 'silent'
  }
  t.plan(2 + cloneUrls.length)
  npm.load(opts, function (err) {
    t.ifError(err, 'npm loaded without error')
    npm.commands.install(['foo/private'], function (err, result) {
      t.match(err.message, /mock fails on purpose/, 'mocked install failed as expected')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
