var p = require("../tar").createParser()
  , fs = require("fs")
  , tar = require("../tar")

p.on("file", function (file) {
  console.error("file start", file.name, file.size, file.extended)
  console.error(file)
  Object.keys(file._raw).forEach(function (f) {
    console.log(f, file._raw[f].toString().replace(/\0+$/, ""))
  })
  file.on("data", function (c) {
    console.error("data", c.toString().replace(/\0+$/, ""))
  })
  file.on("end", function () {
    console.error("end", file.name)
  })
})


var s = fs.createReadStream(__dirname + "/test-generator.tar")
s.on("data", function (c) {
  console.error("stream data", c.toString())
})
s.on("end", function () { console.error("stream end") })
s.on("close", function () { console.error("stream close") })
p.on("end", function () { console.error("parser end") })

s.pipe(p)
