var tap = require("tap")
var normalize = require("../lib/normalize")
var fs = require("fs")
var async = require("async")

var data
var warn

tap.test("consistent normalization", function(t) {
  var entries = [
    'read-package-json.json',
    'http-server.json',
    "movefile.json",
    "node-module_exist.json"
  ]
  var verifyConsistency = function(entryName, next) {
    warn = function(msg) {
      // t.equal("",msg) // uncomment to have some kind of logging of warnings
    }
    var filename = __dirname + "/fixtures/" + entryName
    fs.readFile(filename, function(err, contents) {
      if (err) return next(err)
      data = JSON.parse(contents.toString())
      normalize(data, warn)
      if(data.name == "node-module_exist") {
        t.same(data.bugs.url, "https://gist.github.com/3135914")
      }
      if(data.name == "read-package-json") {
        t.same(data.bugs.url, "https://github.com/isaacs/read-package-json/issues")
      }
      if(data.name == "http-server") {
        t.same(data.bugs.url, "https://github.com/nodejitsu/http-server/issues")
      }
      if(data.name == "movefile") {
        t.same(data.bugs.url, "https://github.com/yazgazan/movefile/issues")
      }
      next(null)
    }) // fs.readFile
  } // verifyConsistency
  async.forEach(entries, verifyConsistency, function(err) {
    if (err) throw err
    t.end()
  })
}) // tap.test
