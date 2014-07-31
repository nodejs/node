var tar = require("../tar.js")
  , fstream = require("fstream")
  , fs = require("fs")

var dir_destination = fs.createWriteStream('dir.tar')

// This must be a "directory"
fstream.Reader({ path: __dirname, type: "Directory" })
  .pipe(tar.Pack({ noProprietary: true }))
  .pipe(dir_destination)