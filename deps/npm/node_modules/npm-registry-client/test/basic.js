var tap = require('tap')
var server = require('./fixtures/server.js')
var RC = require('../')
var rimraf = require("rimraf")
var client = new RC({
    cache: __dirname + '/fixtures/cache'
  , registry: 'http://localhost:' + server.port })
var us = require('./fixtures/underscore/1.3.3/cache.json')
var usroot = require("./fixtures/underscore/cache.json")

tap.test("basic request", function (t) {
  server.expect("/underscore/1.3.3", function (req, res) {
    console.error('got a request')
    res.json(us)
  })

  server.expect("/underscore", function (req, res) {
    console.error('got a request')
    res.json(usroot)
  })

  t.plan(2)
  client.get("/underscore/1.3.3", function (er, data, raw, res) {
    console.error("got response")
    t.deepEqual(data, us)
  })

  client.get("/underscore", function (er, data, raw, res) {
    console.error("got response")
    t.deepEqual(data, usroot)
  })
})
