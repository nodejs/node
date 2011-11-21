var fstream = require("../fstream.js")
var path = require("path")

var r = fstream.Reader({ path: path.dirname(__dirname)
                       , filter: function () {
                           return !this.basename.match(/^\./)
                         }
                       })

console.error(r instanceof fstream.Reader)
console.error(r instanceof require("stream").Stream)
console.error(r instanceof require("events").EventEmitter)
console.error(r.on)

r.on("stat", function () {
  console.error("a %s !!!\t", r.type, r.path)
})

r.on("entries", function (entries) {
  console.error("\t" + entries.join("\n\t"))
})

r.on("entry", function (entry) {
  console.error("a %s !!!\t", entry.type, entry.path)
})

r.on("end", function () {
  console.error("IT'S OVER!!")
})
