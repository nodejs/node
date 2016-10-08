var url = 'http://127.0.0.1:15986/-/whoami'
var auth = new Buffer("user:pass@:!%'").toString('base64')
var badauth = new Buffer("user:wrong").toString('base64')
var test = require('tap').test
var request = require('request')

test('hit whoami endpoint as user', function (t) {
  request({
    url: url,
    headers: {
      authorization: 'Basic ' + auth
    },
    json: true
  }, function (er, res, body) {
    t.same(body, {
      username: 'user'
    })
    t.end()
  })
})

test('hit whoami endpoint as anon', function (t) {
  request({
    url: url,
    json: true
  }, function (er, res, body) {
    t.same(body, { username: null })
    t.end()
  })
})

test('hit whoami with wrong pass', function (t) {
  request({
    url: url,
    headers: {
      authorization: 'Basic ' + badauth
    },
    json: true
  }, function (er, res, body) {
    t.same(body, {
      error: "unauthorized",
      reason: "Name or password is incorrect."
    })
    t.end()
  })
})
