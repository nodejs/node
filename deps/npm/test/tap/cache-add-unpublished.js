var common = require('../common-tap.js')
var test = require('tap').test

var server

var port = common.port
var http = require("http")

var doc = {
  "_id": "superfoo",
  "_rev": "5-d11adeec0fdfea6b96b120610d2bed71",
  "name": "superfoo",
  "time": {
    "modified": "2014-02-18T18:35:02.930Z",
    "created": "2014-02-18T18:34:08.437Z",
    "1.1.0": "2014-02-18T18:34:08.437Z",
    "unpublished": {
      "name": "isaacs",
      "time": "2014-04-30T18:26:45.584Z",
      "tags": {
        "latest": "1.1.0"
      },
      "maintainers": [
        {
          "name": "foo",
          "email": "foo@foo.com"
        }
      ],
      "description": "do lots a foo",
      "versions": [
        "1.1.0"
      ]
    }
  },
  "_attachments": {}
}

test("setup", function (t) {
  server = http.createServer(function(req, res) {
    res.end(JSON.stringify(doc))
  })
  server.listen(port, function() {
    t.end()
  })
})

test("cache add", function (t) {
  common.npm(["cache", "add", "superfoo"], {}, function (er, c, so, se) {
    if (er) throw er
    t.ok(c)
    t.equal(so, "")
    t.similar(se, /404 Not Found: superfoo/)
    t.end()
  })
})

test("cleanup", function (t) {
  server.close(function() {
    t.end()
  })
})
