var tap = require("tap")
var normalize = require("../lib/normalize")
var path = require("path")
var fs = require("fs")
var _ = require("underscore")
var async = require("async")

var data, clonedData
var warn

tap.test("consistent normalization", function(t) {
  path.resolve(__dirname, "./fixtures/read-package-json.json")
  fs.readdir (__dirname + "/fixtures", function (err, entries) {
    // entries = ['coffee-script.json'] // uncomment to limit to a specific file
    verifyConsistency = function(entryName, next) {
      warn = function(msg) { 
        // t.equal("",msg) // uncomment to have some kind of logging of warnings
      }
      filename = __dirname + "/fixtures/" + entryName
      fs.readFile(filename, function(err, contents) {
        if (err) return next(err)
        data = JSON.parse(contents.toString())
        normalize(data, warn)
        clonedData = _.clone(data)
        normalize(data, warn)
        t.deepEqual(clonedData, data,
          "Normalization of " + entryName + " is consistent.")
        next(null)
      }) // fs.readFile
    } // verifyConsistency
    async.forEach(entries, verifyConsistency, function(err) {
      if (err) throw err
      t.end()
    })
  }) // fs.readdir
}) // tap.test