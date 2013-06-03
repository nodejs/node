var server = require('./server')
  , assert = require('assert')
  , request = require('../main.js')


var s = server.createServer()
var ss = server.createSSLServer()
var sUrl = 'http://localhost:' + s.port
var ssUrl = 'https://localhost:' + ss.port

s.listen(s.port, bouncy(s, ssUrl))
ss.listen(ss.port, bouncy(ss, sUrl))

var hits = {}
var expect = {}
var pending = 0
function bouncy (s, server) { return function () {

  var redirs = { a: 'b'
               , b: 'c'
               , c: 'd'
               , d: 'e'
               , e: 'f'
               , f: 'g'
               , g: 'h'
               , h: 'end' }

  var perm = true
  Object.keys(redirs).forEach(function (p) {
    var t = redirs[p]

    // switch type each time
    var type = perm ? 301 : 302
    perm = !perm
    s.on('/' + p, function (req, res) {
      res.writeHead(type, { location: server + '/' + t })
      res.end()
    })
  })

  s.on('/end', function (req, res) {
    var h = req.headers['x-test-key']
    hits[h] = true
    pending --
    if (pending === 0) done()
  })
}}

for (var i = 0; i < 5; i ++) {
  pending ++
  var val = 'test_' + i
  expect[val] = true
  request({ url: (i % 2 ? sUrl : ssUrl) + '/a'
          , headers: { 'x-test-key': val } })
}

function done () {
  assert.deepEqual(hits, expect)
  process.exit(0)
}
