// this example will bundle every dependency
var P = require("../")
P({ path: "./" })
  .on("package", bundleIt)
  .on("entry", function (e) {
    console.error(e.constructor.name, e.path.substr(e.root.dirname.length + 1))
    e.on("package", bundleIt)
  })

function bundleIt (p) {
  p.bundleDependencies = Object.keys(p.dependencies || {})
}

