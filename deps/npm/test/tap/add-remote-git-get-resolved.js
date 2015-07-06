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

  t.equal(
    getResolved('git+ssh://user@bananaboat.com:galbi/blah.git', 'decadacefadabade'),
    'git+ssh://user@bananaboat.com:galbi/blah.git#decadacefadabade',
    'don\'t break non-hosted scp-style locations'
  )

  t.equal(
    getResolved('git+ssh://bananaboat:galbi/blah', 'decadacefadabade'),
    'git+ssh://bananaboat:galbi/blah#decadacefadabade',
    'don\'t break non-hosted scp-style locations'
  )

  t.equal(
    getResolved('git+ssh://git.bananaboat.net/foo', 'decadacefadabade'),
    'git+ssh://git.bananaboat.net/foo#decadacefadabade',
    'don\'t break non-hosted SSH URLs'
  )

  t.equal(
    getResolved('git+ssh://git.bananaboat.net:/foo', 'decadacefadabade'),
    'git+ssh://git.bananaboat.net:/foo#decadacefadabade',
    'don\'t break non-hosted SSH URLs'
  )

  t.equal(
    getResolved('git://gitbub.com/foo/bar.git', 'decadacefadabade'),
    'git://gitbub.com/foo/bar.git#decadacefadabade',
    'don\'t break non-hosted git: URLs'
  )

  t.end()
})
