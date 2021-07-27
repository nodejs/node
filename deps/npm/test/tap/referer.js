var common = require('../common-tap.js')
var test = require('tap').test
var http = require('http')

test('should send referer http header', function (t) {
  http.createServer(function (q, s) {
    t.equal(q.headers.referer, 'install foo')
    s.statusCode = 404
    s.end(JSON.stringify({error: 'whatever'}))
    this.close()
  }).listen(common.port, function () {
    var reg = 'http://localhost:' + common.port
    var args = [ 'install', 'foo', '--registry', reg ]
    common.npm(args, {}, function (er, code) {
      if (er) {
        throw er
      }
      // should not have ended nicely, since we returned an error
      t.ok(code)
      t.end()
    })
  })
})

test('should redact user secret from hook add command', function (t) {
  http.createServer(function (q, s) {
    t.equal(q.headers.referer, 'hook add ~zkat [REDACTED] [REDACTED]')
    s.statusCode = 204
    s.end()
    this.close()
  }).listen(common.port, function () {
    var reg = `http://localhost:${common.port}`
    var args = [ 'hook', 'add', '~zkat', 'https://example.com', 'sekrit', '--registry', reg ]
    common.npm(args, {}, function (er, code) {
      if (er) {
        throw er
      }
      // should not have ended nicely, since we returned an error
      t.ok(code)
      t.end()
    })
  })
})

test('should redact user secret from hook up command', function (t) {
  http.createServer(function (q, s) {
    t.equal(q.headers.referer, 'hook up ~zkat [REDACTED] [REDACTED]')
    s.statusCode = 204
    s.end()
    this.close()
  }).listen(common.port, function () {
    var reg = `http://localhost:${common.port}`
    var args = [ 'hook', 'up', '~zkat', 'https://example.com', 'sekrit', '--registry', reg ]
    common.npm(args, {}, function (er, code) {
      if (er) {
        throw er
      }
      // should not have ended nicely, since we returned an error
      t.ok(code)
      t.end()
    })
  })
})

test('should redact user secret from hook update command', function (t) {
  http.createServer(function (q, s) {
    t.equal(q.headers.referer, 'hook update ~zkat [REDACTED] [REDACTED]')
    s.statusCode = 204
    s.end()
    this.close()
  }).listen(common.port, function () {
    var reg = `http://localhost:${common.port}`
    var args = [ 'hook', 'update', '~zkat', 'https://example.com', 'sekrit', '--registry', reg ]
    common.npm(args, {}, function (er, code) {
      if (er) {
        throw er
      }
      // should not have ended nicely, since we returned an error
      t.ok(code)
      t.end()
    })
  })
})
