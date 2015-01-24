var test = require("tap").test

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

function nop () {}

var URI      = "https://npm.registry:8043/rewrite"
var USERNAME = "username"
var PASSWORD = "password"
var EMAIL    = "n@p.m"
var AUTH     = {
  auth : {
    username : USERNAME,
    password : PASSWORD,
    email    : EMAIL
  }
}

test("adduser call contract", function (t) {
  t.throws(function () {
    client.adduser(undefined, AUTH, nop)
  }, "requires a URI")

  t.throws(function () {
    client.adduser([], AUTH, nop)
  }, "requires URI to be a string")

  t.throws(function () {
    client.adduser(URI, undefined, nop)
  }, "requires params object")

  t.throws(function () {
    client.adduser(URI, "", nop)
  }, "params must be object")

  t.throws(function () {
    client.adduser(URI, AUTH, undefined)
  }, "requires callback")

  t.throws(function () {
    client.adduser(URI, AUTH, "callback")
  }, "callback must be function")

  t.throws(
    function () {
      var params = {
        auth : {
          password : PASSWORD,
          email    : EMAIL
        }
      }
      client.adduser(URI, params, nop)
    },
    { name : "AssertionError", message : "must include username in auth" },
    "auth must include username"
  )

  t.throws(
    function () {
      var params = {
        auth : {
          username : USERNAME,
          email    : EMAIL
        }
      }
      client.adduser(URI, params, nop)
    },
    { name : "AssertionError", message : "must include password in auth" },
    "auth must include password"
  )

  t.throws(
    function () {
      var params = {
        auth : {
          username : USERNAME,
          password : PASSWORD
        }
      }
      client.adduser(URI, params, nop)
    },
    { name : "AssertionError", message : "must include email in auth" },
    "auth must include email"
  )

  t.test("username missing", function (t) {
    var params = {
      auth : {
        username : "",
        password : PASSWORD,
        email    : EMAIL
      }
    }
    client.adduser(URI, params, function (err) {
      t.equal(err && err.message, "No username supplied.", "username must not be empty")
      t.end()
    })
  })

  t.test("password missing", function (t) {
    var params = {
      auth : {
        username : USERNAME,
        password : "",
        email    : EMAIL
      }
    }
    client.adduser(URI, params, function (err) {
      t.equal(
        err && err.message,
        "No password supplied.",
        "password must not be empty"
      )
      t.end()
    })
  })

  t.test("email missing", function (t) {
    var params = {
      auth : {
        username : USERNAME,
        password : PASSWORD,
        email    : ""
      }
    }
    client.adduser(URI, params, function (err) {
      t.equal(
        err && err.message,
        "No email address supplied.",
        "email must not be empty"
      )
      t.end()
    })
  })

  t.test("email malformed", function (t) {
    var params = {
      auth : {
        username : USERNAME,
        password : PASSWORD,
        email    : "lolbutts"
      }
    }
    client.adduser(URI, params, function (err) {
      t.equal(
        err && err.message,
        "Please use a real email address.",
        "email must look like email"
      )
      t.end()
    })
  })

  t.end()
})

test("cleanup", function (t) {
  server.close()
  t.end()
})
