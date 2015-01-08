var test = require("tap").test

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()
var cache = require("./fixtures/underscore/cache.json")

function nop () {}

var URI      = "https://npm.registry:8043/rewrite"
var STARRED  = true
var USERNAME = "username"
var PASSWORD = "%1234@asdf%"
var EMAIL    = "i@izs.me"
var AUTH     = {
  username : USERNAME,
  password : PASSWORD,
  email : EMAIL
}
var PARAMS  = {
  starred : STARRED,
  auth    : AUTH
}

test("star call contract", function (t) {
  t.throws(function () {
    client.star(undefined, PARAMS, nop)
  }, "requires a URI")

  t.throws(function () {
    client.star([], PARAMS, nop)
  }, "requires URI to be a string")

  t.throws(function () {
    client.star(URI, undefined, nop)
  }, "requires params object")

  t.throws(function () {
    client.star(URI, "", nop)
  }, "params must be object")

  t.throws(function () {
    client.star(URI, PARAMS, undefined)
  }, "requires callback")

  t.throws(function () {
    client.star(URI, PARAMS, "callback")
  }, "callback must be function")

  t.throws(
    function () {
      var params = {
        starred : STARRED
      }
      client.star(URI, params, nop)
    },
    { name : "AssertionError", message : "must pass auth to star" },
    "params must include auth"
  )

  t.test("token auth disallowed in star", function (t) {
    var params = {
      auth : {
        token : "lol"
      }
    }
    client.star(URI, params, function (err) {
      t.equal(
        err && err.message,
        "This operation is unsupported for token-based auth",
        "star doesn't support token-based auth"
      )
      t.end()
    })
  })

  t.end()
})

test("star a package", function (t) {
  server.expect("GET", "/underscore?write=true", function (req, res) {
    t.equal(req.method, "GET")

    res.json(cache)
  })

  server.expect("PUT", "/underscore", function (req, res) {
    t.equal(req.method, "PUT")

    var b = ""
    req.setEncoding("utf8")
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      var updated = JSON.parse(b)

      var already  = [
        "vesln", "mvolkmann", "lancehunt", "mikl", "linus", "vasc", "bat",
        "dmalam", "mbrevoort", "danielr", "rsimoes", "thlorenz"
      ]
      for (var i = 0; i < already.length; i++) {
        var current = already[i]
        t.ok(
          updated.users[current],
          current + " still likes this package"
        )
      }
      t.ok(updated.users[USERNAME], "user is in the starred list")

      res.statusCode = 201
      res.json({starred:true})
    })
  })

  var params = {
    starred : STARRED,
    auth : AUTH
  }
  client.star("http://localhost:1337/underscore", params, function (error, data) {
    t.ifError(error, "no errors")
    t.ok(data.starred, "was starred")

    t.end()
  })
})
