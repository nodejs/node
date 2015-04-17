'use strict'
var HostedGit = require('../index')
var test = require('tap').test

test('Bitbucket HTTPS URLs with embedded auth', function (t) {
  t.is(
    HostedGit.fromUrl('https://user:pass@bitbucket.org/user/repo.git').toString(),
    'git+https://user:pass@bitbucket.org/user/repo.git',
    'credentials were included in URL'
  )
  t.is(
    HostedGit.fromUrl('https://user:pass@bitbucket.org/user/repo').toString(),
    'git+https://user:pass@bitbucket.org/user/repo.git',
    'credentials were included in URL'
  )
  t.is(
    HostedGit.fromUrl('git+https://user:pass@bitbucket.org/user/repo.git').toString(),
    'git+https://user:pass@bitbucket.org/user/repo.git',
    'credentials were included in URL'
  )
  t.is(
    HostedGit.fromUrl('git+https://user:pass@bitbucket.org/user/repo').toString(),
    'git+https://user:pass@bitbucket.org/user/repo.git',
    'credentials were included in URL'
  )
  t.end()
})
