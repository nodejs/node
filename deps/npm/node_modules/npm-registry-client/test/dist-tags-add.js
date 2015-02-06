var test = require("tap").test

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

function nop() {}

var BASE_URL = "http://localhost:1337/"
var URI = "/-/package/underscore/dist-tags/test"
var TOKEN = "foo"
var AUTH = {
  token : TOKEN
}
var PACKAGE = "underscore"
var DIST_TAG = "test"
var VERSION = "3.1.3"
var PARAMS = {
  package : PACKAGE,
  distTag : DIST_TAG,
  version : VERSION,
  auth : AUTH
}

test("distTags.add call contract", function (t) {
  t.throws(function () {
    client.distTags.add(undefined, AUTH, nop)
  }, "requires a URI")

  t.throws(function () {
    client.distTags.add([], PARAMS, nop)
  }, "requires URI to be a string")

  t.throws(function () {
    client.distTags.add(BASE_URL, undefined, nop)
  }, "requires params object")

  t.throws(function () {
    client.distTags.add(BASE_URL, "", nop)
  }, "params must be object")

  t.throws(function () {
    client.distTags.add(BASE_URL, PARAMS, undefined)
  }, "requires callback")

  t.throws(function () {
    client.distTags.add(BASE_URL, PARAMS, "callback")
  }, "callback must be function")

  t.throws(
    function () {
      var params = {
        distTag : DIST_TAG,
        version : VERSION,
        auth : AUTH
      }
      client.distTags.add(BASE_URL, params, nop)
    },
    {
      name : "AssertionError",
      message : "must pass package name to distTags.add"
    },
    "distTags.add must include package name"
  )

  t.throws(
    function () {
      var params = {
        package : PACKAGE,
        version : VERSION,
        auth : AUTH
      }
      client.distTags.add(BASE_URL, params, nop)
    },
    {
      name : "AssertionError",
      message : "must pass package distTag name to distTags.add"
    },
    "distTags.add must include dist-tag"
  )

  t.throws(
    function () {
      var params = {
        package : PACKAGE,
        distTag : DIST_TAG,
        auth : AUTH
      }
      client.distTags.add(BASE_URL, params, nop)
    },
    {
      name : "AssertionError",
      message : "must pass version to be mapped to distTag to distTags.add"
    },
    "distTags.add must include version"
  )

  t.throws(
    function () {
      var params = {
        package : PACKAGE,
        distTag : DIST_TAG,
        version : VERSION
      }
      client.distTags.add(BASE_URL, params, nop)
    },
    { name : "AssertionError", message : "must pass auth to distTags.add" },
    "distTags.add must include auth"
  )

  t.end()
})

test("add a new dist-tag to a package", function (t) {
  server.expect("PUT", URI, function (req, res) {
    t.equal(req.method, "PUT")

    var b = ""
    req.setEncoding("utf8")
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      t.doesNotThrow(function () {
        var parsed = JSON.parse(b)
        t.deepEqual(parsed, VERSION)

        res.statusCode = 200
        res.json({ "test" : VERSION })
      }, "got valid JSON from client")
    })
  })

  client.distTags.add(BASE_URL, PARAMS, function (error, data) {
    t.ifError(error, "no errors")
    t.ok(data.test, "dist-tag added")

    t.end()
  })
})
