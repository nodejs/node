var test = require("tap").test

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

function nop() {}

var BASE_URL = "http://localhost:1337/"
var URI = "/-/package/underscore/dist-tags"
var TOKEN = "foo"
var AUTH = {
  token : TOKEN
}
var PACKAGE = "underscore"
var PARAMS = {
  package : PACKAGE,
  auth : AUTH
}

test("distTags.fetch call contract", function (t) {
  t.throws(function () {
    client.distTags.fetch(undefined, AUTH, nop)
  }, "requires a URI")

  t.throws(function () {
    client.distTags.fetch([], PARAMS, nop)
  }, "requires URI to be a string")

  t.throws(function () {
    client.distTags.fetch(BASE_URL, undefined, nop)
  }, "requires params object")

  t.throws(function () {
    client.distTags.fetch(BASE_URL, "", nop)
  }, "params must be object")

  t.throws(function () {
    client.distTags.fetch(BASE_URL, PARAMS, undefined)
  }, "requires callback")

  t.throws(function () {
    client.distTags.fetch(BASE_URL, PARAMS, "callback")
  }, "callback must be function")

  t.throws(
    function () {
      var params = {
        auth : AUTH
      }
      client.distTags.fetch(BASE_URL, params, nop)
    },
    {
      name : "AssertionError",
      message : "must pass package name to distTags.fetch"
    },
    "distTags.fetch must include package name"
  )

  t.throws(
    function () {
      var params = {
        package : PACKAGE
      }
      client.distTags.fetch(BASE_URL, params, nop)
    },
    { name : "AssertionError", message : "must pass auth to distTags.fetch" },
    "distTags.fetch must include auth"
  )

  t.end()
})

test("fetch dist-tags for a package", function (t) {
  server.expect("GET", URI, function (req, res) {
    t.equal(req.method, "GET")

    var b = ""
    req.setEncoding("utf8")
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      t.notOk(b, "no request body")

      res.statusCode = 200
      res.json({ a : "1.0.0", b : "2.0.0", _etag : "xxx" })
    })
  })

  client.distTags.fetch(BASE_URL, PARAMS, function (error, data) {
    t.ifError(error, "no errors")
    t.same(data, { a : "1.0.0", b : "2.0.0" }, "etag filtered from response")

    t.end()
  })
})
