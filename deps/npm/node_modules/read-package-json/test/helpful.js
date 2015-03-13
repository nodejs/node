// vim: set softtabstop=16 shiftwidth=16:
var tap = require("tap")
var readJson = require("../")
var path = require("path")
var fs = require("fs")
var p = path.resolve(__dirname, "fixtures/erroneous.json")

var expect = {}

console.error("readme test")
tap.test("readme test", function (t) {
                readJson(p, function (er, data) {
                                t.ok(er instanceof Error)
                                t.ok(er.message.match(/Unexpected token '\\''/))
				t.end()
                })
})
