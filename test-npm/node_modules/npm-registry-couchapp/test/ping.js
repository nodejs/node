var http = require('http')
var parse = require('parse-json-response')
var test = require('tap').test

var reg = 'http://admin:admin@localhost:15986/registry'
var ping = reg + '/_design/scratch/_show/ping'
var rw = reg + '/_design/scratch/_rewrite/-/ping'

var wanted = {
  host: 'localhost:15986'
, ok: true
, username: 'admin'
, peer: '127.0.0.1'
}

;[ping, ping + '/any', rw, rw + '/any'].forEach(function (uri) {
  test('ping ' + uri, function (t) {
    t.plan(2)
    http.get(uri, parse(function (er, data, res) {
      t.ok(!er)
      t.same(data, wanted)
      t.end()
    }))
  })
})
