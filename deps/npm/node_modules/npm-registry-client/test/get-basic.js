var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

var us = require("./fixtures/underscore/1.3.3/cache.json")
var usroot = require("./fixtures/underscore/cache.json")

tap.test("basic request", function (t) {
  server.expect("/underscore/1.3.3", function (req, res) {
    res.json(us)
  })

  server.expect("/underscore", function (req, res) {
    res.json(usroot)
  })

  server.expect("/@bigco%2funderscore", function (req, res) {
    res.json(usroot)
  })

  t.plan(3)
  client.get("http://localhost:1337/underscore/1.3.3", null, function (er, data) {
    t.deepEqual(data, us)
  })

  client.get("http://localhost:1337/underscore", null, function (er, data) {
    t.deepEqual(data, usroot)
  })

  client.get("http://localhost:1337/@bigco%2funderscore", null, function (er, data) {
    t.deepEqual(data, usroot)
  })
})
