'use strict'
var HostedGit = require('../index')
var test = require('tap').test

test('HTTPS GitHub URL with embedded auth -- generally not a good idea', function (t) {
  function verify (host, label, branch) {
    var hostinfo = HostedGit.fromUrl(host)
    var hash = branch ? '#' + branch : ''
    t.ok(hostinfo, label)
    if (!hostinfo) return
    t.is(hostinfo.https(), 'git+https://user:pass@github.com/111/222.git' + hash, label + ' -> https')
    t.is(hostinfo.git(), 'git://user:pass@github.com/111/222.git' + hash, label + ' -> git')
    t.is(hostinfo.browse(), 'https://github.com/111/222' + (branch ? '/tree/' + branch : ''), label + ' -> browse')
    t.is(hostinfo.bugs(), 'https://github.com/111/222/issues', label + ' -> bugs')
    t.is(hostinfo.docs(), 'https://github.com/111/222' + (branch ? '/tree/' + branch : '') + '#readme', label + ' -> docs')
    t.is(hostinfo.ssh(), 'git@github.com:111/222.git' + hash, label + ' -> ssh')
    t.is(hostinfo.sshurl(), 'git+ssh://git@github.com/111/222.git' + hash, label + ' -> sshurl')
    t.is(hostinfo.shortcut(), 'github:111/222' + hash, label + ' -> shortcut')
    t.is(hostinfo.file('C'), 'https://user:pass@raw.githubusercontent.com/111/222/' + (branch || 'master') + '/C', label + ' -> file')
  }

  // insecure protocols
  verify('git://user:pass@github.com/111/222', 'git')
  verify('git://user:pass@github.com/111/222.git', 'git.git')
  verify('git://user:pass@github.com/111/222#branch', 'git#branch', 'branch')
  verify('git://user:pass@github.com/111/222.git#branch', 'git.git#branch', 'branch')

  verify('https://user:pass@github.com/111/222', 'https')
  verify('https://user:pass@github.com/111/222.git', 'https.git')
  verify('https://user:pass@github.com/111/222#branch', 'https#branch', 'branch')
  verify('https://user:pass@github.com/111/222.git#branch', 'https.git#branch', 'branch')

  verify('http://user:pass@github.com/111/222', 'http')
  verify('http://user:pass@github.com/111/222.git', 'http.git')
  verify('http://user:pass@github.com/111/222#branch', 'http#branch', 'branch')
  verify('http://user:pass@github.com/111/222.git#branch', 'http.git#branch', 'branch')

  t.end()
})
