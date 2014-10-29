var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")

var nerfed = "//localhost:" + server.port + "/:"

var configuration = {}
configuration[nerfed + "_authToken"] = "not-bad-meaning-bad-but-bad-meaning-wombat"

var client = common.freshClient(configuration)

var cache = require("./fixtures/underscore/cache.json")

var VERSION = "1.3.2"
var MESSAGE = "uhhh"

tap.test("deprecate a package", function (t) {
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

      var undeprecated  = [
        "1.0.3", "1.0.4", "1.1.0", "1.1.1", "1.1.2", "1.1.3", "1.1.4", "1.1.5", "1.1.6",
        "1.1.7", "1.2.0", "1.2.1", "1.2.2", "1.2.3", "1.2.4", "1.3.0", "1.3.1", "1.3.3"
      ]
      for (var i = 0; i < undeprecated.length; i++) {
        var current = undeprecated[i]
        t.notEqual(
          updated.versions[current].deprecated,
          MESSAGE,
          current + " not deprecated"
        )
      }

      t.equal(
        updated.versions[VERSION].deprecated,
        MESSAGE,
        VERSION + " deprecated"
      )
      res.statusCode = 201
      res.json({deprecated:true})
    })
  })

  client.deprecate(common.registry + "/underscore", VERSION, MESSAGE, function (er, data) {
    t.ifError(er)
    t.ok(data.deprecated, "was deprecated")

    t.end()
  })
})
