var readInstalled = require("../read-installed.js")
var util = require("util")
console.error("testing")

var called = 0
readInstalled(process.cwd(), function (er, map) {
  console.error(called ++)
  if (er) return console.error(er.stack || er.message)
  cleanup(map)
  console.error(util.inspect(map, true, 10, true))
})

var seen = []
function cleanup (map) {
  if (seen.indexOf(map) !== -1) return
  seen.push(map)
  for (var i in map) switch (i) {
    case "_id":
    case "path":
    case "extraneous": case "invalid":
    case "dependencies": case "name":
      continue
    default: delete map[i]
  }
  var dep = map.dependencies
//    delete map.dependencies
  if (dep) {
//      map.dependencies = dep
    for (var i in dep) if (typeof dep[i] === "object") {
      cleanup(dep[i])
    }
  }
  return map
}
