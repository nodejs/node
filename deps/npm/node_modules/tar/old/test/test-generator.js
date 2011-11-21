// pipe this file to tar vt

var Generator = require("../generator")
  , fs = require("fs")
  , path = require("path")
  , ohm = fs.createReadStream(path.resolve(__dirname, "tar-files/Ω.txt"))
  , foo = path.resolve(__dirname, "tar-files/foo.js")
  , gen = Generator.create({cwd: __dirname})

gen.pipe(process.stdout)
gen.append(ohm)
//gen.append(foo)
gen.end()
