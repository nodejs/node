var common = require('../common-tap.js')
var mr = require('npm-registry-mock')

var test = require('tap').test
var rimraf = require('rimraf')
var fs = require('fs')
var path = require('path')
var fakeBrowser = path.join(common.pkg, '_script.sh')
var outFile = path.join(common.pkg, '_output')
var opts = { cwd: common.pkg }
var mkdirp = require('mkdirp')

common.pendIfWindows('This is trickier to convert without opening new shells')

test('setup', function (t) {
  mkdirp.sync(common.pkg)
  var s = '#!/usr/bin/env bash\n' +
          'echo "$@" > ' + JSON.stringify(common.pkg) + '/_output\n'
  fs.writeFileSync(fakeBrowser, s, 'ascii')
  fs.chmodSync(fakeBrowser, '0755')
  t.pass('made script')
  t.end()
})

test('npm repo underscore', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm([
      'repo', 'underscore',
      '--registry=' + common.registry,
      '--loglevel=silent',
      '--browser=' + fakeBrowser
    ], opts, function (err, code, stdout, stderr) {
      t.ifError(err, 'repo command ran without error')
      t.equal(code, 0, 'exit ok')
      var res = fs.readFileSync(outFile, 'ascii')
      s.close()
      t.equal(res, 'https://github.com/jashkenas/underscore\n')
      rimraf.sync(outFile)
      t.end()
    })
  })
})

test('npm repo optimist - github (https://)', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm([
      'repo', 'optimist',
      '--registry=' + common.registry,
      '--loglevel=silent',
      '--browser=' + fakeBrowser
    ], opts, function (err, code, stdout, stderr) {
      t.ifError(err, 'repo command ran without error')
      t.equal(code, 0, 'exit ok')
      var res = fs.readFileSync(outFile, 'ascii')
      s.close()
      t.equal(res, 'https://github.com/substack/node-optimist\n')
      rimraf.sync(outFile)
      t.end()
    })
  })
})

test('npm repo npm-test-peer-deps - no repo', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm([
      'repo', 'npm-test-peer-deps',
      '--registry=' + common.registry,
      '--loglevel=silent',
      '--browser=' + fakeBrowser
    ], opts, function (err, code, stdout, stderr) {
      t.ifError(err, 'repo command ran without error')
      t.equal(code, 1, 'exit not ok')
      s.close()
      t.end()
    })
  })
})

test('npm repo test-repo-url-http - non-github (http://)', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm([
      'repo', 'test-repo-url-http',
      '--registry=' + common.registry,
      '--loglevel=silent',
      '--browser=' + fakeBrowser
    ], opts, function (err, code, stdout, stderr) {
      t.ifError(err, 'repo command ran without error')
      t.equal(code, 0, 'exit ok')
      var res = fs.readFileSync(outFile, 'ascii')
      s.close()
      t.equal(res, 'http://gitlab.com/evanlucas/test-repo-url-http\n')
      rimraf.sync(outFile)
      t.end()
    })
  })
})

test('npm repo test-repo-url-https - non-github (https://)', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm([
      'repo', 'test-repo-url-https',
      '--registry=' + common.registry,
      '--loglevel=silent',
      '--browser=' + fakeBrowser
    ], opts, function (err, code, stdout, stderr) {
      t.ifError(err, 'repo command ran without error')
      t.equal(code, 0, 'exit ok')
      var res = fs.readFileSync(outFile, 'ascii')
      s.close()
      t.equal(res, 'https://gitlab.com/evanlucas/test-repo-url-https\n')
      rimraf.sync(outFile)
      t.end()
    })
  })
})

test('npm repo test-repo-url-ssh - non-github (ssh://)', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm([
      'repo', 'test-repo-url-ssh',
      '--registry=' + common.registry,
      '--loglevel=silent',
      '--browser=' + fakeBrowser
    ], opts, function (err, code, stdout, stderr) {
      t.ifError(err, 'repo command ran without error')
      t.equal(code, 0, 'exit ok')
      var res = fs.readFileSync(outFile, 'ascii')
      s.close()
      t.equal(res, 'https://gitlab.com/evanlucas/test-repo-url-ssh\n')
      rimraf.sync(outFile)
      t.end()
    })
  })
})

test('cleanup', function (t) {
  fs.unlinkSync(fakeBrowser)
  t.pass('cleaned up')
  t.end()
})
