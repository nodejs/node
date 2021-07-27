var test = require('tap').test
var common = require('../common-tap.js')

var opts = { cwd: process.cwd() }

test('npm asdf should return exit code 1', function (t) {
  common.npm(['asdf'], opts, function (err, code, stdout, stderr) {
    if (err) throw err
    t.ok(code, 'exit code should not be zero')
    if (stdout.trim()) t.comment(stdout.trim())
    if (stderr.trim()) t.comment(stderr.trim())
    t.end()
  })
})

test('npm help should return exit code 0', function (t) {
  common.npm(['help'], opts, function (err, code, stdout, stderr) {
    if (err) throw err
    t.equal(code, 0, 'exit code should be 0')
    if (stdout.trim()) t.comment(stdout.trim())
    if (stderr.trim()) t.comment(stderr.trim())
    t.end()
  })
})

test('npm help fadf should return exit code 0', function (t) {
  common.npm(['help', 'fadf'], opts, function (err, code, stdout, stderr) {
    if (err) throw err
    t.equal(code, 0, 'exit code should be 0')
    if (stdout.trim()) t.comment(stdout.trim())
    if (stderr.trim()) t.comment(stderr.trim())
    t.end()
  })
})
