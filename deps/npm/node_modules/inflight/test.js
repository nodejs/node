var test = require('tap').test
var inf = require('./inflight.js')


function req (key, cb) {
  cb = inf(key, cb)
  if (cb) setTimeout(function () {
    cb(key)
    cb(key)
  })
  return cb
}

test('basic', function (t) {
  var calleda = false
  var a = req('key', function (k) {
    t.notOk(calleda)
    calleda = true
    t.equal(k, 'key')
    if (calledb) t.end()
  })
  t.ok(a, 'first returned cb function')

  var calledb = false
  var b = req('key', function (k) {
    t.notOk(calledb)
    calledb = true
    t.equal(k, 'key')
    if (calleda) t.end()
  })

  t.notOk(b, 'second should get falsey inflight response')
})
