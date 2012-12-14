// vim: set softtabstop=16 shiftwidth=16:
var tap = require("tap")
var readJson = require("../")
var path = require("path")
var fs = require("fs")

console.error("BOM test")
tap.test("BOM test", function (t) {
                var p = path.resolve(__dirname, "fixtures/bom.json")
                readJson(p, function (er, data) {
                                if (er) throw er;
                                p = path.resolve(__dirname, "fixtures/nobom.json")
                                readJson(p, function (er, data2) {
                                                if (er) throw er;
                                                t.deepEqual(data, data2)
                                                t.end()
                                })
                })
})
