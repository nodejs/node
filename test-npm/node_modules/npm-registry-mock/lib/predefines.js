var fs = require("fs")
var path = require("path")
var fixtures = path.resolve(__dirname, "..", "fixtures")
var pkgs = fs.readdirSync(fixtures).filter(function(f) {
  return !f.match(/\.json$/)
})

var routes = pkgs.map(function(p) {
  var pdir = fixtures + "/" + p
  var vers = fs.readdirSync(pdir).filter(function(v) {
    return v !== "-"
  }).map(function(v) {
    return pdir + "/" + v.replace(/\.json$/, "")
  })

  var tgzdir = pdir + "/-"
  var tgzs = fs.readdirSync(tgzdir).map(function (t) {
    return tgzdir + "/" + t
  })
  return [pdir].concat(vers).concat(tgzs)
}).reduce(function(set, pkg) {
  return set.concat(pkg)
}, []).reduce(function(gets, route) {
  route = route.substr(fixtures.length)
  if (route) {
    gets[route] = [200]
  }

  return gets
}, {})

var predefinedMocks = exports.predefinedMocks = {
  "get": routes,
  "put": {},
  "post": {},
  "head": {},
  "delete": {}
}
