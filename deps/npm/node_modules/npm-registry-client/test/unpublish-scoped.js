var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")

var nerfed = "//localhost:" + server.port + "/:"

var configuration = {}
configuration[nerfed + "_authToken"]  = "of-glad-tidings"

var client = common.freshClient(configuration)

var cache = require("./fixtures/@npm/npm-registry-client/cache.json")

var REV = "/-rev/213-0a1049cf56172b7d9a1184742c6477b9"
var VERSION = "3.0.6"

tap.test("unpublish a package", function (t) {
  server.expect("GET", "/@npm%2fnpm-registry-client?write=true", function (req, res) {
    t.equal(req.method, "GET")

    res.json(cache)
  })

  server.expect("PUT", "/@npm%2fnpm-registry-client" + REV, function (req, res) {
    t.equal(req.method, "PUT")

    var b = ""
    req.setEncoding("utf-8")
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      var updated = JSON.parse(b)
      t.notOk(updated.versions[VERSION])
    })

    res.json(cache)
  })

  server.expect("GET", "/@npm%2fnpm-registry-client", function (req, res) {
    t.equal(req.method, "GET")

    res.json(cache)
  })

  server.expect("DELETE", "/@npm%2fnpm-registry-client/-/@npm%2fnpm-registry-client-" + VERSION + ".tgz" + REV, function (req, res) {
    t.equal(req.method, "DELETE")

    res.json({unpublished:true})
  })

  client.unpublish("http://localhost:1337/@npm%2fnpm-registry-client", VERSION, function (error) {
    t.ifError(error, "no errors")

    t.end()
  })
})
