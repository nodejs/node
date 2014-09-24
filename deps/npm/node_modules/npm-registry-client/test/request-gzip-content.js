var zlib = require("zlib")
var tap = require("tap")

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient({
  "fetch-retries"          : 1,
  "fetch-retry-mintimeout" : 10,
  "fetch-retry-maxtimeout" : 100
})

var TEST_URL = "http://localhost:1337/some-package-gzip/1.2.3"

var pkg = {
  _id: "some-package-gzip@1.2.3",
  name: "some-package-gzip",
  version: "1.2.3"
}

zlib.gzip(JSON.stringify(pkg), function (err, pkgGzip) {
  tap.test("request gzip package content", function (t) {
    t.ifError(err, "example package compressed")

    server.expect("GET", "/some-package-gzip/1.2.3", function (req, res) {
      res.statusCode = 200
      res.setHeader("Content-Encoding", "gzip")
      res.setHeader("Content-Type", "application/json")
      res.end(pkgGzip)
    })

    client.get(TEST_URL, null, function (er, data) {
      if (er) throw er
      t.deepEqual(data, pkg)
      t.end()
    })
  })

  tap.test("request wrong gzip package content", function (t) {
    server.expect("GET", "/some-package-gzip-error/1.2.3", function (req, res) {
      res.statusCode = 200
      res.setHeader("Content-Encoding", "gzip")
      res.setHeader("Content-Type", "application/json")
      res.end(new Buffer("wrong gzip content"))
    })

    client.get(TEST_URL, null, function (er) {
      t.ok(er)
      t.end()
    })
  })
})
