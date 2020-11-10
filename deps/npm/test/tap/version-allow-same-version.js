const fs = require('graceful-fs')
const path = require('path')
const t = require('tap')
const common = require('../common-tap.js')
const npm = require('../../')
const pkg = common.pkg
const cache = common.cache
const npmrc = path.resolve(pkg, './.npmrc')
const configContents = 'sign-git-tag=false\n'

t.test('setup', t => {
  process.chdir(pkg)
  fs.writeFileSync(npmrc, configContents, 'ascii')
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Lucas Theisen',
    name: 'version-allow-same-version',
    version: '0.0.1',
    description: 'Test for npm version without --allow-same-version'
  }), 'utf8')
  npm.load({cache: cache, 'allow-same-version': false, registry: common.registry}, t.end)
})

t.test('without --allow-same-version', t => {
  npm.config.set('allow-same-version', false)

  const version = require('../../lib/version')

  const commit1 = version.buildCommitArgs()
  const commit2 = version.buildCommitArgs([ 'commit' ])
  const commit3 = version.buildCommitArgs([ 'commit', '-m', 'some commit message' ])

  t.same(commit1, [ 'commit' ])
  t.same(commit2, [ 'commit' ])
  t.same(commit3, [ 'commit', '-m', 'some commit message' ])

  const tag = version.buildTagFlags()

  t.same(tag, '-m')

  npm.commands.version(['0.0.1'], function (err) {
    t.isa(err, Error, 'got an error')
    t.like(err.message, /Version not changed/)
    t.end()
  })
})

t.test('with --allow-same-version', t => {
  npm.config.set('allow-same-version', true)

  const version = require('../../lib/version')

  const commit1 = version.buildCommitArgs()
  const commit2 = version.buildCommitArgs([ 'commit' ])
  const commit3 = version.buildCommitArgs([ 'commit', '-m', 'some commit message' ])

  t.same(commit1, [ 'commit', '--allow-empty' ])
  t.same(commit2, [ 'commit', '--allow-empty' ])
  t.same(commit3, [ 'commit', '--allow-empty', '-m', 'some commit message' ])

  const tag = version.buildTagFlags()

  t.same(tag, '-fm')

  npm.commands.version(['0.0.1'], function (err) {
    if (err) {
      throw err
    }
    t.end()
  })
})
