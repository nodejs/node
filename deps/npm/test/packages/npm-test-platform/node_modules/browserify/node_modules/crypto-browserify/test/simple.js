var crypto = require('crypto')
var cryptoB = require('../')
var assert = require('assert')


function assertSame (fn) {
  fn(crypto, function (err, expected) {
    fn(cryptoB, function (err, actual) {
      assert.equal(actual, expected)
    })
  })
}

assertSame(function (crypto, cb) {
  cb(null, crypto.createHash('sha1').update('hello', 'utf-8').digest('hex'))
})

assert.equal(cryptoB.randomBytes(10).length, 10)

cryptoB.randomBytes(10, function(ex, bytes) {
  assert.ifError(ex)
  bytes.forEach(function(bite) {
    assert.equal(typeof bite, 'number')
  });
})