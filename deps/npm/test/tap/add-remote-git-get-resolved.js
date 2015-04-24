'use strict'
var test = require('tap').test

var npm = require('../../lib/npm.js')
var common = require('../common-tap.js')

test('setup', function (t) {
  var opts = {
    registry: common.registry,
    loglevel: 'silent'
  }
  npm.load(opts, function (er) {
    t.ifError(er, 'npm loaded without error')

    t.end()
  })
})

test('add-remote-git#get-resolved git: passthru', function (t) {
  var getResolved = require('../../lib/cache/add-remote-git.js').getResolved

  verify('git:github.com/foo/repo')
  verify('git:github.com/foo/repo.git')
  verify('git://github.com/foo/repo#decadacefadabade')
  verify('git://github.com/foo/repo.git#decadacefadabade')

  function verify (uri) {
    t.equal(
      getResolved(uri, 'decadacefadabade'),
      'git://github.com/foo/repo.git#decadacefadabade',
      uri + ' normalized to canonical form git://github.com/foo/repo.git#decadacefadabade'
    )
  }
  t.end()
})

test('add-remote-git#get-resolved SSH', function (t) {
  var getResolved = require('../../lib/cache/add-remote-git.js').getResolved

  t.comment('tests for https://github.com/npm/npm/issues/7961')
  verify('git@github.com:foo/repo')
  verify('git@github.com:foo/repo#master')
  verify('git+ssh://git@github.com/foo/repo#master')
  verify('git+ssh://git@github.com/foo/repo#decadacefadabade')

  function verify (uri) {
    t.equal(
      getResolved(uri, 'decadacefadabade'),
      'git+ssh://git@github.com/foo/repo.git#decadacefadabade',
      uri + ' normalized to canonical form git+ssh://git@github.com/foo/repo.git#decadacefadabade'
    )
  }
  t.end()
})

test('add-remote-git#get-resolved HTTPS', function (t) {
  var getResolved = require('../../lib/cache/add-remote-git.js').getResolved

  verify('https://github.com/foo/repo')
  verify('https://github.com/foo/repo#master')
  verify('git+https://github.com/foo/repo.git#master')
  verify('git+https://github.com/foo/repo#decadacefadabade')

  function verify (uri) {
    t.equal(
      getResolved(uri, 'decadacefadabade'),
      'git+https://github.com/foo/repo.git#decadacefadabade',
      uri + ' normalized to canonical form git+https://github.com/foo/repo.git#decadacefadabade'
    )
  }
  t.end()
})

test('add-remote-git#get-resolved edge cases', function (t) {
  var getResolved = require('../../lib/cache/add-remote-git.js').getResolved

  t.notOk(
    getResolved('git@bananaboat.com:galbi.git', 'decadacefadabade'),
    'non-hosted Git SSH non-URI strings are invalid'
  )

  t.equal(
    getResolved('git+ssh://git.bananaboat.net/foo', 'decadacefadabade'),
    'git+ssh://git.bananaboat.net/foo#decadacefadabade',
    'don\'t break non-hosted SSH URLs'
  )

  t.equal(
    getResolved('git://gitbub.com/foo/bar.git', 'decadacefadabade'),
    'git://gitbub.com/foo/bar.git#decadacefadabade',
    'don\'t break non-hosted git: URLs'
  )

  t.comment('test for https://github.com/npm/npm/issues/3224')
  t.equal(
    getResolved('git+ssh://git@git.example.com:my-repo.git#9abe82cb339a70065e75300f62b742622774693c', 'decadacefadabade'),
    'git+ssh://git@git.example.com:my-repo.git#decadacefadabade',
    'preserve weird colon in semi-standard ssh:// URLs'
  )
  t.end()
})
