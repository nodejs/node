var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../')
var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var exec = require('child_process').exec

var pkg = __dirname + '/install-cli'
var NPM_BIN = __dirname + '/../../bin/npm-cli.js'

function hasOnlyAscii (s) {
  return /^[\000-\177]*$/.test(s) ;
}

test('does not use unicode with --unicode false', function (t) {
  t.plan(3)
  mr(common.port, function (s) {
    exec('node ' + NPM_BIN + ' install --unicode false read', {
      cwd: pkg
    }, function(err, stdout) {
      t.ifError(err)
      t.ok(stdout, stdout.length)
      t.ok(hasOnlyAscii(stdout))
      s.close()
    })
  })
})

test('cleanup', function (t) {
  mr(common.port, function (s) {
    exec('node ' + NPM_BIN + ' uninstall read', {
      cwd: pkg
    }, function(err, stdout) {
      s.close()
    })
  })
  t.end()
})
