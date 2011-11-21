var argv = process.argv
if (argv.length < 3) {
  console.error("Usage: read-package.json <file> [<fields> ...]")
  process.exit(1)
}

var fs = require("fs")
  , file = argv[2]
  , readJson = require("../lib/utils/read-json")

readJson(file, function (er, data) {
  if (er) throw er
  if (argv.length === 3) console.log(data)
  else argv.slice(3).forEach(function (field) {
    field = field.split(".")
    var val = data
    field.forEach(function (f) {
      val = val[f]
    })
    console.log(val)
  })
})
