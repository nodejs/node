if (process.platform === 'win32') {
  console.error('skipping test, because windows and shebangs')
  process.exit(0)
}

var common = require('../common-tap.js')
var mr = require('npm-registry-mock')

var test = require('tap').test
var rimraf = require('rimraf')
var fs = require('fs')
var path = require('path')
var outFile = path.join(__dirname, '/_output')

var opts = { cwd: __dirname }

test('setup', function (t) {
  var s = '#!/usr/bin/env bash\n' +
          'echo \"$@\" > ' + JSON.stringify(__dirname) + '/_output\n'
  fs.writeFileSync(__dirname + '/_script.sh', s, 'ascii')
  fs.chmodSync(__dirname + '/_script.sh', '0755')
  t.pass('made script')
  t.end()
})

test('npm repo underscore', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm([
      'repo', 'underscore',
      '--registry=' + common.registry,
      '--loglevel=silent',
      '--browser=' + __dirname + '/_script.sh'
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
      '--browser=' + __dirname + '/_script.sh'
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
      '--browser=' + __dirname + '/_script.sh'
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
      '--browser=' + __dirname + '/_script.sh'
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
      '--browser=' + __dirname + '/_script.sh'
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
      '--browser=' + __dirname + '/_script.sh'
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
  fs.unlinkSync(__dirname + '/_script.sh')
  t.pass('cleaned up')
  t.end()
})
