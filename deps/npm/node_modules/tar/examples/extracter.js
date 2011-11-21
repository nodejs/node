var tar = require("../tar.js")
  , fs = require("fs")

fs.createReadStream(__dirname + "/../test/fixtures/c.tar")
  .pipe(tar.Extract({ path: __dirname + "/extract" }))
  .on("error", function (er) {
    console.error("error here")
  })
  .on("end", function () {
    console.error("done")
  })
