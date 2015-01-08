var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

var cache = require("./fixtures/@npm/npm-registry-client/cache.json")

var REV     = "/-rev/213-0a1049cf56172b7d9a1184742c6477b9"
var PACKAGE = "/@npm%2fnpm-registry-client"
var URI     = common.registry + PACKAGE
var TOKEN   = "of-glad-tidings"
var VERSION = "3.0.6"
var AUTH    = {
  token : TOKEN
}
var PARAMS  = {
  version : VERSION,
  auth    : AUTH
}

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

  server.expect("GET", PACKAGE, function (req, res) {
    t.equal(req.method, "GET")

    res.json(cache)
  })

  server.expect("DELETE", PACKAGE+"/-"+PACKAGE+"-"+VERSION+".tgz"+REV, function (req, res) {
    t.equal(req.method, "DELETE")

    res.json({unpublished:true})
  })

  client.unpublish(URI, PARAMS, function (er) {
    t.ifError(er, "no errors")

    t.end()
  })
})
