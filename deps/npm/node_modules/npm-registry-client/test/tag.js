var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")

var nerfed = "//localhost:" + server.port + "/:"

var configuration = {}
configuration[nerfed + "username"]  = "username"
configuration[nerfed + "_password"] = new Buffer("%1234@asdf%").toString("base64")
configuration[nerfed + "email"]     = "i@izs.me"

var client = common.freshClient(configuration)

tap.test("tag a package", function (t) {
  server.expect("PUT", "/underscore/not-lodash", function (req, res) {
    t.equal(req.method, "PUT")

    var b = ""
    req.setEncoding("utf8")
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      var updated = JSON.parse(b)

      t.deepEqual(updated, {"1.3.2":{}})

      res.statusCode = 201
      res.json({tagged:true})
    })
  })

  client.tag("http://localhost:1337/underscore", {"1.3.2":{}}, "not-lodash", function (error, data) {
    t.ifError(error, "no errors")
    t.ok(data.tagged, "was tagged")

    t.end()
  })
})
