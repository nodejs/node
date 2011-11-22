
module.exports = set
var get = require("./get.js")
  , processJson = require("./read-json.js").processJson
function set (obj, key, val) {
  for (var i in obj) {
    if (i.toLowerCase() === key.toLowerCase()) return obj[i] = val
  }
  obj[key] = val
  if (!val) return
  // if it's a package set, then assign all the versions.
  if (val.versions) return Object.keys(val.versions).forEach(function (v) {
    if (typeof val.versions[v] !== "object") return
    set(obj, key+"@"+v, val.versions[v])
  })
  // Note that this doesn't put the dist-tags there, only updates the versions
  if (key === val.name+"@"+val.version) {
    processJson(val)
    var reg = get(obj, val.name) || {}
    reg.name = reg._id = val.name
    set(obj, val.name, reg)
    reg.versions = get(reg, "versions") || {}
    if (!get(reg.versions, val.version)) set(reg.versions, val.version, val)
  }
}
