var common = require('./common.js')
var path = require('path')
var url = require('url')
var test = require('tap').test
var reg = 'http://127.0.0.1:15986/'
var request = require('request')
var accessurl = url.resolve(reg, '/-/package/package/access')

test('not implemented endpoints should 501', function (t) {
  var methods = ['get', 'put', 'post', 'del']
  var urls = [ '/-/package/package/access' ]

  t.plan(methods.length * urls.length)

  urls.forEach(function (u) {
    methods.forEach(function (method) {
      t.test(method + ' ' + u, function (t) {
        request[method]({
          url: url.resolve(reg, u),
          json: true
        }, function (er, res, body) {
          t.equal(res.statusCode, 501)
          t.same(body, {
            error: "Not Implemented",
            reason: "This server does not support this endpoint"
          })
          t.end()
        })
      })
    })
  })
})
