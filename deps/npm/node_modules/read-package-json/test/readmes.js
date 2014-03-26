// vim: set softtabstop=16 shiftwidth=16:
var tap = require("tap")
var readJson = require("../")
var path = require("path")
var fs = require("fs")
var p = path.resolve(__dirname, "fixtures/readmes/package.json")

var expect = {}
var expect = {
  "name" : "readmes",
  "version" : "99.999.999999999",
  "readme" : "*markdown*\n",
  "readmeFilename" : "README.md",
  "description" : "*markdown*",
  "_id" : "readmes@99.999.999999999"
}

console.error("readme test")
tap.test("readme test", function (t) {
                readJson(p, function (er, data) {
                                if (er) throw er;
                                test(t, data)
                })
})

function test(t, data) {
                t.deepEqual(data, expect)
                t.end()
}
