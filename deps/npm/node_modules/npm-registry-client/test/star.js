var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")

var DEP_USER = "username"

var nerfed = "//localhost:" + server.port + "/:"

var configuration = {}
configuration[nerfed + "username"]  = DEP_USER
configuration[nerfed + "_password"] = new Buffer("%1234@asdf%").toString("base64")
configuration[nerfed + "email"]     = "i@izs.me"

var client = common.freshClient(configuration)

var cache = require("./fixtures/underscore/cache.json")

tap.test("star a package", function (t) {
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
      t.ok(updated.users[DEP_USER], "user is in the starred list")

      res.statusCode = 201
      res.json({starred:true})
    })
  })

  client.star("http://localhost:1337/underscore", true, function (error, data) {
    t.ifError(error, "no errors")
    t.ok(data.starred, "was starred")

    t.end()
  })
})
