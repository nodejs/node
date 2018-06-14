var common = require('../common-tap.js')
common.pendIfWindows('not working because Windows and shebangs')

var mr = require('npm-registry-mock')

var test = require('tap').test
var rimraf = require('rimraf')
var fs = require('fs')
var path = require('path')
var join = path.join
var outFile = path.join(__dirname, '/_output')

var opts = { cwd: __dirname }

test('setup', function (t) {
  var s = '#!/usr/bin/env bash\n' +
          'echo "$@" > ' + JSON.stringify(__dirname) + '/_output\n'
  fs.writeFileSync(join(__dirname, '/_script.sh'), s, 'ascii')
  fs.chmodSync(join(__dirname, '/_script.sh'), '0755')
  t.pass('made script')
  t.end()
})

test('npm bugs underscore', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        'bugs', 'underscore',
        '--registry=' + common.registry,
        '--loglevel=silent',
        '--browser=' + join(__dirname, '/_script.sh')
      ],
      opts,
      function (err, code, stdout, stderr) {
        t.ifError(err, 'bugs ran without issue')
        t.notOk(stderr, 'should have no stderr')
        t.equal(code, 0, 'exit ok')
        var res = fs.readFileSync(outFile, 'ascii')
        s.close()
        t.equal(res, 'https://github.com/jashkenas/underscore/issues\n')
        rimraf.sync(outFile)
        t.end()
      }
    )
  })
})

test('npm bugs optimist - github (https://)', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        'bugs', 'optimist',
        '--registry=' + common.registry,
        '--loglevel=silent',
        '--browser=' + join(__dirname, '/_script.sh')
      ],
      opts,
      function (err, code, stdout, stderr) {
        t.ifError(err, 'bugs ran without issue')
        t.notOk(stderr, 'should have no stderr')
        t.equal(code, 0, 'exit ok')
        var res = fs.readFileSync(outFile, 'ascii')
        s.close()
        t.equal(res, 'https://github.com/substack/node-optimist/issues\n')
        rimraf.sync(outFile)
        t.end()
      }
    )
  })
})

test('npm bugs npm-test-peer-deps - no repo', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        'bugs', 'npm-test-peer-deps',
        '--registry=' + common.registry,
        '--loglevel=silent',
        '--browser=' + join(__dirname, '/_script.sh')
      ],
      opts,
      function (err, code, stdout, stderr) {
        t.ifError(err, 'bugs ran without issue')
        t.notOk(stderr, 'should have no stderr')
        t.equal(code, 0, 'exit ok')
        var res = fs.readFileSync(outFile, 'ascii')
        s.close()
        t.equal(res, 'https://www.npmjs.org/package/npm-test-peer-deps\n')
        rimraf.sync(outFile)
        t.end()
      }
    )
  })
})

test('npm bugs test-repo-url-http - non-github (http://)', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        'bugs', 'test-repo-url-http',
        '--registry=' + common.registry,
        '--loglevel=silent',
        '--browser=' + join(__dirname, '/_script.sh')
      ],
      opts,
      function (err, code, stdout, stderr) {
        t.ifError(err, 'bugs ran without issue')
        t.notOk(stderr, 'should have no stderr')
        t.equal(code, 0, 'exit ok')
        var res = fs.readFileSync(outFile, 'ascii')
        s.close()
        t.equal(res, 'https://www.npmjs.org/package/test-repo-url-http\n')
        rimraf.sync(outFile)
        t.end()
      }
    )
  })
})

test('npm bugs test-repo-url-https - gitlab (https://)', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        'bugs', 'test-repo-url-https',
        '--registry=' + common.registry,
        '--loglevel=silent',
        '--browser=' + join(__dirname, '/_script.sh')
      ],
      opts,
      function (err, code, stdout, stderr) {
        t.ifError(err, 'bugs ran without issue')
        t.notOk(stderr, 'should have no stderr')
        t.equal(code, 0, 'exit ok')
        var res = fs.readFileSync(outFile, 'ascii')
        s.close()
        t.equal(res, 'https://gitlab.com/evanlucas/test-repo-url-https/issues\n')
        rimraf.sync(outFile)
        t.end()
      }
    )
  })
})

test('npm bugs test-repo-url-ssh - gitlab (ssh://)', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        'bugs', 'test-repo-url-ssh',
        '--registry=' + common.registry,
        '--loglevel=silent',
        '--browser=' + join(__dirname, '/_script.sh')
      ],
      opts,
      function (err, code, stdout, stderr) {
        t.ifError(err, 'bugs ran without issue')
        t.notOk(stderr, 'should have no stderr')
        t.equal(code, 0, 'exit ok')
        var res = fs.readFileSync(outFile, 'ascii')
        s.close()
        t.equal(res, 'https://gitlab.com/evanlucas/test-repo-url-ssh/issues\n')
        rimraf.sync(outFile)
        t.end()
      }
    )
  })
})

test('cleanup', function (t) {
  fs.unlinkSync(join(__dirname, '/_script.sh'))
  t.pass('cleaned up')
  t.end()
})
