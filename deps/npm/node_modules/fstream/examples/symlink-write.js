var fstream = require("../fstream.js")

fstream
  .Writer({ path: "path/to/symlink"
          , linkpath: "./file"
          , isSymbolicLink: true
          , mode: "0755" // octal strings supported
          })
  .end()
