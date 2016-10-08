'use strict'
var test = require('tap').test

var npm = require('../../lib/npm.js')
var common = require('../common-tap.js')

var normalizeGitUrl = require('normalize-git-url')
var getResolved = null

/**
 * Note: This is here because `normalizeGitUrl` is usually called
 * before getResolved is, and receives *that* URL.
 */
function tryGetResolved (uri, treeish) {
  return getResolved(normalizeGitUrl(uri).url, treeish)
}

test('setup', function (t) {
  var opts = {
    registry: common.registry,
    loglevel: 'silent'
  }
  npm.load(opts, function (er) {
    t.ifError(er, 'npm loaded without error')
    getResolved = require('../../lib/cache/add-remote-git.js').getResolved
    t.end()
  })
})

test('add-remote-git#get-resolved git: passthru', function (t) {
  verify('git:github.com/foo/repo')
  verify('git:github.com/foo/repo.git')
  verify('git://github.com/foo/repo#decadacefadabade')
  verify('git://github.com/foo/repo.git#decadacefadabade')

  function verify (uri) {
    t.equal(
      tryGetResolved(uri, 'decadacefadabade'),
      'git://github.com/foo/repo.git#decadacefadabade',
      uri + ' normalized to canonical form git://github.com/foo/repo.git#decadacefadabade'
    )
  }
  t.end()
})

test('add-remote-git#get-resolved SSH', function (t) {
  t.comment('tests for https://github.com/npm/npm/issues/7961')
  verify('git@github.com:foo/repo')
  verify('git@github.com:foo/repo#master')
  verify('git+ssh://git@github.com/foo/repo#master')
  verify('git+ssh://git@github.com/foo/repo#decadacefadabade')

  function verify (uri) {
    t.equal(
      tryGetResolved(uri, 'decadacefadabade'),
      'git+ssh://git@github.com/foo/repo.git#decadacefadabade',
      uri + ' normalized to canonical form git+ssh://git@github.com/foo/repo.git#decadacefadabade'
    )
  }
  t.end()
})

test('add-remote-git#get-resolved HTTPS', function (t) {
  verify('https://github.com/foo/repo')
  verify('https://github.com/foo/repo#master')
  verify('git+https://github.com/foo/repo.git#master')
  verify('git+https://github.com/foo/repo#decadacefadabade')
  // DEPRECATED
  // this is an invalid URL but we normalize it
  // anyway. Users shouldn't use this in the future. See note
  // below for how this affected non-hosted URLs.
  // See https://github.com/npm/npm/issues/8881
  verify('git+https://github.com:foo/repo.git#master')

  function verify (uri) {
    t.equal(
      tryGetResolved(uri, 'decadacefadabade'),
      'git+https://github.com/foo/repo.git#decadacefadabade',
      uri + ' normalized to canonical form git+https://github.com/foo/repo.git#decadacefadabade'
    )
  }
  t.end()
})

test('add-remote-git#get-resolved edge cases', function (t) {
  t.equal(
    tryGetResolved('git+ssh://user@bananaboat.com:galbi/blah.git', 'decadacefadabade'),
    'git+ssh://user@bananaboat.com:galbi/blah.git#decadacefadabade',
    'don\'t break non-hosted scp-style locations'
  )

  t.equal(
    tryGetResolved('git+ssh://bananaboat:galbi/blah', 'decadacefadabade'),
    'git+ssh://bananaboat:galbi/blah#decadacefadabade',
    'don\'t break non-hosted scp-style locations'
  )

  // DEPRECATED
  // When we were normalizing all git URIs, git+https: was being
  // automatically converted to ssh:. Some users were relying
  // on this funky behavior, so after removing the aggressive
  // normalization from non-hosted URIs, we brought this back.
  // See https://github.com/npm/npm/issues/8881
  t.equal(
    tryGetResolved('git+https://bananaboat:galbi/blah', 'decadacefadabade'),
    'git+https://bananaboat/galbi/blah#decadacefadabade',
    'don\'t break non-hosted scp-style locations'
  )

  t.equal(
    tryGetResolved('git+ssh://git.bananaboat.net/foo', 'decadacefadabade'),
    'git+ssh://git.bananaboat.net/foo#decadacefadabade',
    'don\'t break non-hosted SSH URLs'
  )

  t.equal(
    tryGetResolved('git+ssh://git.bananaboat.net:/foo', 'decadacefadabade'),
    'git+ssh://git.bananaboat.net:/foo#decadacefadabade',
    'don\'t break non-hosted SSH URLs'
  )

  t.equal(
    tryGetResolved('git://gitbub.com/foo/bar.git', 'decadacefadabade'),
    'git://gitbub.com/foo/bar.git#decadacefadabade',
    'don\'t break non-hosted git: URLs'
  )

  t.end()
})
