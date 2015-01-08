var resolve = require("path").resolve
var createReadStream = require("graceful-fs").createReadStream
var readFileSync = require("graceful-fs").readFileSync

var tap = require("tap")
var cat = require("concat-stream")
var Negotiator = require("negotiator")

var server = require("./lib/server.js")
var common = require("./lib/common.js")

var tgz = resolve(__dirname, "./fixtures/underscore/1.3.3/package.tgz")

tap.test("fetch accepts github api's json", function (t) {
  server.expect("/underscore/-/underscore-1.3.3", function (req, res) {
    t.equal(req.method, "GET", "got expected method")

    var negotiator = new Negotiator(req)

    // fetching a tarball from `api.github.com` returns a 415 error if json is
    // not accepted
    if (negotiator.mediaTypes().indexOf("application/vnd.github+json") === -1) {
      res.writeHead(415, {
        "Content-Type" : "application/json"
      })
    }
    else {
      res.writeHead(302, {
        "Content-Type" : "text/html",
        "Location"     : "/underscore/-/underscore-1.3.3.tgz"
      })
    }

    res.end()
  })

  server.expect("/underscore/-/underscore-1.3.3.tgz", function (req, res) {
    t.equal(req.method, "GET", "got expected method")

    res.writeHead(200, {
      "Content-Type"     : "application/x-tar",
      "Content-Encoding" : "gzip"
    })

    createReadStream(tgz).pipe(res)
  })

  var client = common.freshClient()
  var defaulted = {}
  client.fetch(
    "http://localhost:1337/underscore/-/underscore-1.3.3",
    defaulted,
    function (er, res) {
      t.ifError(er, "loaded successfully")

      var sink = cat(function (data) {
        t.deepEqual(data, readFileSync(tgz))
        t.end()
      })

      res.on("error", function (error) {
        t.ifError(error, "no errors on stream")
      })

      res.pipe(sink)
    }
  )
})
