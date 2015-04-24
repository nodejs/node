'use strict'
var HostedGit = require('../index')
var test = require('tap').test

test('fromUrl(bitbucket url)', function (t) {
  function verify (host, label, branch) {
    var hostinfo = HostedGit.fromUrl(host)
    var hash = branch ? '#' + branch : ''
    t.ok(hostinfo, label)
    if (!hostinfo) return
    t.is(hostinfo.https(), 'git+https://bitbucket.org/111/222.git' + hash, label + ' -> https')
    t.is(hostinfo.browse(), 'https://bitbucket.org/111/222' + (branch ? '/src/' + branch : ''), label + ' -> browse')
    t.is(hostinfo.docs(), 'https://bitbucket.org/111/222' + (branch ? '/src/' + branch : '') + '#readme', label + ' -> docs')
    t.is(hostinfo.ssh(), 'git@bitbucket.org:111/222.git' + hash, label + ' -> ssh')
    t.is(hostinfo.sshurl(), 'git+ssh://git@bitbucket.org/111/222.git' + hash, label + ' -> sshurl')
    t.is(hostinfo.shortcut(), 'bitbucket:111/222' + hash, label + ' -> shortcut')
    t.is(hostinfo.file('C'), 'https://bitbucket.org/111/222/raw/' + (branch || 'master') + '/C', label + ' -> file')
  }

  require('./lib/standard-tests')(verify, 'bitbucket.org', 'bitbucket')

  t.end()
})
