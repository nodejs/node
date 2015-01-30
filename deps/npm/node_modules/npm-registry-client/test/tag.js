var test = require("tap").test

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

function nop () {}

var URI      = "http://localhost:1337/underscore"
var USERNAME = "username"
var PASSWORD = "%1234@asdf%"
var EMAIL    = "i@izs.me"
var VERSION  = "1.3.2"
var TAG      = "not-lodash"
var AUTH     = {
  username : USERNAME,
  password : PASSWORD,
  email    : EMAIL
}
var PARAMS = {
  tag     : TAG,
  version : VERSION,
  auth    : AUTH
}

test("tag call contract", function (t) {
  t.throws(function () {
    client.tag(undefined, AUTH, nop)
  }, "requires a URI")

  t.throws(function () {
    client.tag([], AUTH, nop)
  }, "requires URI to be a string")

  t.throws(function () {
    client.tag(URI, undefined, nop)
  }, "requires params object")

  t.throws(function () {
    client.tag(URI, "", nop)
  }, "params must be object")

  t.throws(function () {
    client.tag(URI, AUTH, undefined)
  }, "requires callback")

  t.throws(function () {
    client.tag(URI, AUTH, "callback")
  }, "callback must be function")

  t.throws(
    function () {
      var params = {
        tag : TAG,
        auth : AUTH
      }
      client.tag(URI, params, nop)
    },
    { name : "AssertionError", message : "must pass version to tag" },
    "tag must include version"
  )

  t.throws(
    function () {
      var params = {
        version : VERSION,
        auth : AUTH
      }
      client.tag(URI, params, nop)
    },
    { name : "AssertionError", message : "must pass tag name to tag" },
    "tag must include name"
  )

  t.throws(
    function () {
      var params = {
        version : VERSION,
        tag : TAG
      }
      client.tag(URI, params, nop)
    },
    { name : "AssertionError", message : "must pass auth to tag" },
    "params must include auth"
  )

  t.end()
})

test("tag a package", function (t) {
  server.expect("PUT", "/underscore/not-lodash", function (req, res) {
    t.equal(req.method, "PUT")

    var b = ""
    req.setEncoding("utf8")
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      var updated = JSON.parse(b)

      t.deepEqual(updated, "1.3.2")

      res.statusCode = 201
      res.json({tagged:true})
    })
  })

  client.tag(URI, PARAMS, function (error, data) {
    t.ifError(error, "no errors")
    t.ok(data.tagged, "was tagged")

    t.end()
  })
})
