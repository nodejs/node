#!/usr/bin/env node
var fs = require("fs")
  , path = require("path")
  , root = path.resolve(__dirname, "..")
  , glob = require("glob")
  , conversion = { "cli": 1, "api": 3, "files": 5, "misc": 7 }

glob(root + "/{README.md,doc/*/*.md}", function (er, files) {
  if (er)
    throw er
  output(files.map(function (f) {
    var b = path.basename(f)
    if (b === "README.md")
      return [0, b]
    if (b === "index.md")
      return null
    var s = conversion[path.basename(path.dirname(f))]
    return [s, f]
  }).filter(function (f) {
    return f
  }).sort(function (a, b) {
    return (a[0] === b[0])
           ? ( path.basename(a[1]) === "npm.md" ? -1
             : path.basename(b[1]) === "npm.md" ? 1
             : a[1] > b[1] ? 1 : -1 )
           : a[0] - b[0]
  }))
})

return

function output (files) {
  console.log(
    "npm-index(7) -- Index of all npm documentation\n" +
    "==============================================\n")

  writeLines(files, 0)
  writeLines(files, 1, "Command Line Documentation")
  writeLines(files, 3, "API Documentation")
  writeLines(files, 5, "Files")
  writeLines(files, 7, "Misc")
}

function writeLines (files, sxn, heading) {
  if (heading)
    console.log("# %s\n", heading)
  files.filter(function (f) {
    return f[0] === sxn
  }).forEach(writeLine)
}


function writeLine (sd) {
  var sxn = sd[0] || 1
    , doc = sd[1]
    , d = path.basename(doc, ".md")

  var content = fs.readFileSync(doc, "utf8").split("\n")[0].split("-- ")[1]

  console.log("## %s(%d)\n", d, sxn)
  console.log(content + "\n")
}
