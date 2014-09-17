var fs
try { fs = require('graceful-fs') } catch (e) { fs = require('fs') }
var test = require('tap').test
var glob = require('../')

test('mock fs', function(t) {
  fs.readdir = function(path, cb) {
    process.nextTick(function() {
      cb(new Error('mock fs.readdir error'))
    })
  }
  t.pass('mocked')
  t.end()
})

test('error callback', function(t) {
  glob('*', function(err, res) {
    t.ok(err, 'expecting mock error')
    t.end()
  })
})
